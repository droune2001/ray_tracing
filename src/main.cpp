#include <iostream>
#include <fstream>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib>
#include <float.h>
#include <random>
#include <chrono>
#include <iomanip> // set::setprecision
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <assert.h>
#include <utility> // std::swap in c++11

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include "stb_image_write.h"

#ifndef PI
#    define PI 3.14159f
#endif

std::default_random_engine generator;
std::uniform_real_distribution<float> distribution(0.0f,1.0f);
#define RAN01() distribution(generator)

#define internal static
#define global static
#define local_persist static

#include "vec3.h"
#include "perlin.h"
#include "ray.h"
#include "aabb.h"
#include "utils.h"
#include "camera.h"
#include "hitable.h"
#include "hitable_list.h"
#include "transforms.h"
#include "bvh.h"
#include "texture.h"
#include "material.h"
#include "sphere.h"
#include "plane.h"
#include "box.h"
#include "thread_pool.h"

// NOTE(nfauvet): pgcd(1920,1080) = 120
// 120 = 2*2*2*3*5
#define OUT_WIDTH 1920
#define OUT_HEIGHT 1080
#define NB_SAMPLES 300 // samples per pixel for AA
#define RECURSE_DEPTH 5
#define TILE_WIDTH 120
#define TILE_HEIGHT 1
#define NB_THREADS 12

hitable *mega_big_scene_end_of_book1();
hitable *simple_scene();
hitable *two_perlin_spheres();
hitable* cornell_box();

vec3 color( const ray &r, hitable *world, int depth )
{
    hit_record rec = {};
    if (world->hit(r, 0.001f, FLT_MAX, rec))
    {
        ray scattered;
        vec3 attenuation;
        vec3 emitted = rec.mat_ptr->emitted( rec.u, rec.v, rec.p );
        if ((depth < RECURSE_DEPTH) && rec.mat_ptr->scatter(r, rec, attenuation, scattered))
        {
            // recursive
            return emitted + attenuation * color(scattered, world, depth+1);
        }
        else
        {
            return emitted;
        }
    }
    else
    {
        /*
        // BACKGROUND COLOR
        vec3 unit_direction = unit_vector(r.direction());
        // Y from [-1;1] to [0;1]
        float t = 0.5f * (unit_direction.y() + 1.0f);
        // gradient from blue to white
        return (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);
        */
        return vec3(0,0,0);
    }
}

global std::mutex g_console_mutex;
global int g_total_nb_tiles = 0;
global int g_nb_tiles_finished = 0;
global float g_percent_complete = 0.0f;

struct compute_tile_task : public task
{
    compute_tile_task() : task() {}
    
    virtual ~compute_tile_task() override
    {
        std::unique_lock<std::mutex> g(g_console_mutex);
        ++g_nb_tiles_finished;
        g_percent_complete = 100.0f *
            (float)g_nb_tiles_finished /
            (float)g_total_nb_tiles;
        
        std::cout << std::fixed << std::setprecision(2) << g_percent_complete << "%\r";
    }
    
    virtual void run() override
    {
        for( int j = tile_height-1; j >= 0; --j )
        {
            int y_in_texture_space = tile_origin_y + j;
            int y_in_buffer_space = (image_height - 1) - y_in_texture_space;
            unsigned int *line_buffer_ptr =
                shared_buffer +
                y_in_buffer_space * image_width +
                tile_origin_x;
            
            for( int i = 0; i < tile_width; ++i )
            {
                vec3 col(0,0,0);
                for ( int s = 0; s < samples; ++s ) // multisample
                {
                    float u = float(tile_origin_x + i + RAN01()) / float(image_width);
                    float v = float(tile_origin_y + j + RAN01()) / float(image_height);
                    
                    ray r = cam->get_ray(u,v);
                    col += color(r, world, 0);
                }
                // resolve AA
                col /= (float)samples;
                
                // gamma correction
                col = vec3(sqrtf(col[0]), sqrtf(col[1]), sqrtf(col[2]) );
                unsigned int ir = unsigned int(255.99f*col.r());
                unsigned int ig = unsigned int(255.99f*col.g());
                unsigned int ib = unsigned int(255.99f*col.b());
                
                // 0xABGR
                *line_buffer_ptr++ = ( 0xff000000 | (ib << 16) | (ig << 8) | (ir << 0) );
            }
        }
    }
    
    virtual void showTask() override
    {
        //std::unique_lock<std::mutex> g(g_console_mutex);
        //std::cout << "thread " << "( " << tile_origin_x << ", " << tile_origin_y << ")"
        //<< " id(" << std::this_thread::get_id() << ") working...\n";
    }
    
    int tile_origin_x = 0;
    int tile_origin_y = 0;
    int tile_width = 1;
    int tile_height = 1;
    int image_width = 1;
    int image_height = 1;
    int samples = 1;
    unsigned int *shared_buffer;
    hitable *world;
    camera *cam;
};

int main( int argc, char **argv )
{
    int nx = OUT_WIDTH;
    int ny = OUT_HEIGHT;
    int ns = NB_SAMPLES;
    const char *out_filename = "out.png";
    
    std::cout << "Output file: \"" << out_filename << "\"\n";
    std::cout << "Resolution: " << nx << "x" << ny << "\n";
    std::cout << "Samples per pixel: " << ns << "\n";
    std::cout << "Bounce depth: " << RECURSE_DEPTH << "\n";
    std::cout << "Number of Threads: " << NB_THREADS << "\n";
    
    unsigned int *image_buffer = new unsigned int[nx*ny];
    unsigned int *buffer_ptr = image_buffer;
    
    float time0 = 0.0f;
    float time1 = 1.0f;
    
    // camera setup
    /*
    vec3 eye = vec3( 8.0f, 1.5f, 3.0f );//vec3( 13.0f, 2.0f, 3.0f );//vec3( 8.0f, 1.5f, 3.0f );
    vec3 lookat = vec3( 0.0f, 1.0f, 0.0f );
    vec3 up = vec3(0,1,0);
    float dist_to_focus = 10; //(eye-lookat).length(); // 10
    float aperture = 0.0f;//2.0f;0.0f
    float vfov = 35.0f;
    camera cam(eye, lookat, up, vfov, float(nx) / float(ny), aperture, dist_to_focus, time0, time1);
    */
    
    // cornell camera
    camera cam(
        vec3( 278.0f, 278.0f, -800.0f ), 
        vec3( 278.0f, 278.0f,  278.0f ), 
        vec3( 0.0f, 1.0f, 0.0f ), 
        40.0f, 
        float(nx)/float(ny), 
        0.0f, 
        800.0f,
        time0, 
        time1 );
    
    hitable *world = cornell_box();
    bvh_node *bvh_root = new bvh_node(
        ((hitable_list*)world)->list,
        ((hitable_list*)world)->list_size,
        time0, time1 );
    
    // percent compute
    int nb_pixels = nx * ny;
    int pixels_per_percent = nb_pixels / 100;
    int current_nb_pixels = 0;
    int percent = 0;
    
    // tile setup
    int tile_width = TILE_WIDTH;
    int tile_height = TILE_HEIGHT;
    int nb_tile_x = nx / tile_width;
    int nb_tile_y = ny / tile_height;
    int remainder_x = nx - ( nb_tile_x * tile_width );
    int remainder_y = ny - ( nb_tile_y * tile_height );
    g_total_nb_tiles = nb_tile_x * nb_tile_y;
    g_nb_tiles_finished = 0;
    g_percent_complete = 0;
    // TODO(nfauvet): compute remainder
    
    std::cout << "nb_tile_x: " << nb_tile_x << "\n";
    std::cout << "nb_tile_y: " << nb_tile_y << "\n";
    std::cout << "total_nb_tiles: " << g_total_nb_tiles << "\n";
    
    thread_pool *pool = new thread_pool(NB_THREADS);
    
    auto time_start = std::chrono::high_resolution_clock::now();
    for ( int j = nb_tile_y - 1; j >=0; --j ) // top to bottom
    {
        for ( int i = 0; i < nb_tile_x; ++i ) // left to right
        {
            compute_tile_task *task = new compute_tile_task();
            task->tile_origin_x = i * tile_width;
            task->tile_origin_y = j * tile_height; // bottom left corner of a tile
            task->tile_width = tile_width;
            task->tile_height = tile_height;
            task->image_width = nx;
            task->image_height = ny;
            task->samples = ns;
            task->shared_buffer = image_buffer;
            task->world = (hitable*)bvh_root;//world;
            task->cam = &cam;
            
            {
                //std::unique_lock<std::mutex> g(g_console_mutex);
                //std::cout << "create task at "
                //<< "( " << task->tile_origin_x << ", " << task->tile_origin_y << ")"
                //<< "\n";
            }
            
            pool->addTask( task );
        }
    }
    
    // wait
    pool->finish();
    delete pool;
    
    std::cout << "\n";
    
    auto time_end = std::chrono::high_resolution_clock::now();
    
    std::cout
        << std::fixed << std::setprecision(2)
        << "Time: "
        << std::chrono::duration<double, std::milli>(time_end-time_start).count()
        << "ms\n";
    
    std::cout << "Writing file.\n";
    
    int res = stbi_write_png(
        out_filename,
        nx, ny, 4,
        (void*)image_buffer,
        4*nx); // row stride
    
    delete []image_buffer;
    
    return 0;
}

// SCENES -----------------------------------------

hitable *mega_big_scene_end_of_book1()
{
    
    int n = 500;//500
    int surf_radius = ((int)sqrtf((float)n)) / 2 - 1;
    
    hitable **list = new hitable*[n+1];
    list[0] = new sphere(vec3(0,-1000,0), 1000, new lambertian(new constant_texture(vec3(0.5,0.5,0.5))));
    int i = 1;
    for( int a = -surf_radius; a < surf_radius; ++a )
    {
        for( int b = -surf_radius; b < surf_radius; ++b )
        {
            float choose_mat = RAN01();
            vec3 center( a + 0.9f * RAN01(), 0.2f, b + 0.9f * RAN01() );
            // keep only spheres outside the middle where we have the big ones
            if ( (center - vec3(4.0f, 0.2f, 0.0f)).length() > 0.9f )
            {
                if ( choose_mat < 0.8f ) // diffuse
                {
                    list[i++] = new moving_sphere(
                        center,center + vec3(0.0f, 0.5f*RAN01(), 0.0f),
                        0.0f, 1.0f,
                        0.2f,
                        new lambertian( new constant_texture(vec3(RAN01()*RAN01(),
                                                                  RAN01()*RAN01(),
                                                                  RAN01()*RAN01()))));
                }
                else if ( choose_mat < 0.95 ) // metal
                {
                    list[i++] = new sphere(
                        center, 0.2f,
                        new metal( new constant_texture( vec3(0.5f * ( 1.0f + RAN01()),
                                                              0.5f * ( 1.0f + RAN01()),
                                                              0.5f * ( 1.0f + RAN01()))),
                                  0.5f * RAN01()));
                }
                else // glass
                {
                    list[i++] = new sphere(center, 0.2f, new dielectric(1.5f));
                }
            }
        }
    }
    
    list[i++] = new sphere(vec3(0.0f,1.0f,0.0f), 1.0f, new dielectric(1.5f));
    list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new lambertian(new constant_texture(vec3(0.4f, 0.2f, 0.1f))));
    list[i++] = new sphere(vec3(4.0f,1.0f,0.0f),1.0f, new metal(new constant_texture(vec3(0.7f, 0.6f, 0.5f)), 0.0f));
    
    return new hitable_list(list, i);
    
}


hitable *simple_scene()
{
    int nx, ny, nn;
    unsigned char *tex_data = stbi_load("../data/earth.jpg", &nx, &ny, &nn, 0);
    material *textured_lambert_mat = new lambertian( new image_texture(tex_data, nx, ny));
    
    hitable **list = new hitable*[10];
    int i = 0;
    list[i++] = new sphere(vec3(0,-1000,0), 1000, new lambertian(new constant_texture(vec3(0.5,0.5,0.5))));
    list[i++] = new sphere(vec3(4.0f,1.0f,0.0f), 1.0f, new dielectric(1.5f));
    list[i++] = new sphere(vec3(0.0f,1.0f,0.0f), 1.0f, textured_lambert_mat );
    //list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new lambertian( new checker_texture(new constant_texture( vec3(0.4f, 0.2f, 0.1f)), new constant_texture( vec3(0.1f, 0.4f, 0.1f)))));
    list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new metal(new constant_texture(vec3(0.7f, 0.6f, 0.5f)), 0.0f));
    list[i++] = new sphere(vec3(0, 6, 0), 3, new diffuse_light(new constant_texture(vec3(4,4,3))));
    list[i++] = new xy_rect(3, 5, 1, 3, -2, new diffuse_light(new constant_texture(vec3(4,4,4))));
    return new hitable_list(list, i);
}

hitable *two_perlin_spheres()
{
    texture *pertex = new noise_texture(2.0f);
    hitable **list = new hitable*[2];
    list[0] = new sphere(vec3(0,-1000,0), 1000, new lambertian( pertex ) );
    list[1] = new sphere(vec3(0.0f,2.0f,0.0f), 2.0f, new lambertian( pertex ) );
    
    return new hitable_list(list, 2);
}

hitable *cornell_box()
{
    hitable **list = new hitable*[8];
    int i = 0;
    material *red   = new lambertian( new constant_texture(vec3(0.65f,0.05f,0.05f)));
    material *white = new lambertian( new constant_texture(vec3(0.73f,0.73f,0.73f)));
    material *green = new lambertian( new constant_texture(vec3(0.12f,0.45f,0.15f)));
    material *light = new diffuse_light( new constant_texture(vec3(15,15,15)));
    
    list[i++] = new flip_normals(new yz_rect(0,555,0,555,555, green)); // left
    list[i++] = new yz_rect(0,555,0,555,  0, red);                     // right
    list[i++] = new flip_normals(new xz_rect(0,555,0,555,555, white)); // top
    list[i++] = new xz_rect(213,343,227,332,554, light);               // light
    list[i++] = new xz_rect(0,555,0,555,0, white);                     // bottom
    list[i++] = new flip_normals(new xy_rect(0,555,0,555,555, white)); // back
    list[i++] = new box(vec3(130.0f, 0.0f, 65.0f), vec3(295.0f, 165.0f, 230.0f), white);
    list[i++] = new box(vec3(265.0f, 0.0f, 295.0f), vec3(430.0f, 330.0f, 460.0f), white);
    
    return new hitable_list(list, i);
}

// eof
