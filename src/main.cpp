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
#include "../ext/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include "../ext/stb_image_write.h"

#include "../ext/cxxopts.hpp"

#ifndef PI
#    define PI 3.14159f
#endif

#define internal static
#define global static
#define local_persist static

global std::default_random_engine generator;
global std::uniform_real_distribution<float> distribution(0.0f,1.0f);
#define RAN01() distribution(generator)

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
#include "volume.h"
#include "sphere.h"
#include "plane.h"
#include "box.h"
#include "thread_pool.h"

// NOTE(nfauvet): pgcd(1920,1080) = 120
// 120 = 2*2*2*3*5
#define OUT_WIDTH 1920
#define OUT_HEIGHT 1080
#define NB_SAMPLES 1000 // samples per pixel for AA
#define RECURSE_DEPTH 50
#define TILE_WIDTH 12
#define TILE_HEIGHT 12
#define NB_THREADS 4

hitable *mega_big_scene_end_of_book1();
hitable *mega_big_scene_end_of_book2();
hitable *simple_scene();
hitable *another_simple();
hitable *two_perlin_spheres();
hitable *cornell_box();
hitable *cornell_box_volumes();

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
                col.clamp01();
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
    cxxopts::Options options( "raytracer", "nfauvet raytracer" );
    options.add_options()
        ( "w,width",   "Output image width", cxxopts::value<int>()->default_value( "1920" ) )
        ( "h,height",  "Output image height", cxxopts::value<int>()->default_value( "1080" ) )
        ( "s,samples", "Number of samples per pixel", cxxopts::value<int>()->default_value( "300" ) )
        ( "r,recurse", "Number of bounces", cxxopts::value<int>()->default_value( "1" )->implicit_value( "50" ) )
        ( "t,threads", "Number of threads", cxxopts::value<int>()->default_value( "1" ) )
        ( "windowed",  "Show a preview window", cxxopts::value<bool>()->default_value( "false" ) )
        ( "i,input",   "Input filename", cxxopts::value<std::string>() )
        ( "o,output",  "Output filename", cxxopts::value<std::string>()->default_value( "out.png" ) )
        ( "p,passes",  "Output passes as separate files", cxxopts::value<bool>()->default_value( "false" ) )
        ( "m,multi",   "Output one file for each sample", cxxopts::value<bool>()->default_value( "false" ) )
        ( "rx",        "ROI start x (from left)", cxxopts::value<int>()->default_value( "0" ) )
        ( "ry",        "ROI start y (from top)", cxxopts::value<int>()->default_value( "0" ) )
        ( "rw",        "ROI width", cxxopts::value<int>()->default_value( "1" ) )
        ( "rh",        "ROI height", cxxopts::value<int>()->default_value( "1" ) )
        ( "x,norender","Do not render. Just print info.", cxxopts::value<bool>()->default_value( "false" ) )
        ;
    
    options.parse( argc, argv );
    
    struct 
    {
        int nx; //
        int ny; //
        int ns; //
        int bounces;
        int threads;
        bool windowed;
        std::string in_filename;
        std::string out_filename;
        bool passes;
        bool out_separate;
        int rx;
        int ry;
        int rw;
        int rh;
    } o;
    
    // parse
    o.nx = options["width"].as<int>();
    o.ny = options["height"].as<int>();
    o.ns = options["samples"].as<int>();
    o.bounces = options["recurse"].as<int>();
    o.threads = options["threads"].as<int>();
    o.windowed = options["windowed"].as<bool>();
    
    std::cout << std::endl;
    std::cout << "Resolution: " << o.nx << "x" << o.ny << "\n";
    std::cout << "Samples per pixel: " << o.ns << "\n";
    std::cout << "Bounce depth: " << o.bounces << "\n";
    std::cout << "Number of Threads: " << o.threads << "\n";
    std::cout << "Windowed: " << o.windowed << "\n";
    std::cout << "Input file: \"" << o.in_filename << "\"\n";
    std::cout << "Output file: \"" << o.out_filename << "\"\n";
    
    if ( options.count("x") )
    {
        std::cout << "No Rendering. Exiting.\n";
        return 0;
    }
    
    
    
    
    unsigned int *image_buffer = new unsigned int[o.nx * o.ny];
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
    
    
    /*
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
    */
    
    /* 
    // mega book2 scene camera
    camera cam(
        vec3( 350.0f, 278.0f, -450.0f ), 
        vec3( 180.0f, 278.0f,  278.0f ), 
        vec3( 0.0f, 1.0f, 0.0f ), 
        45.0f, 
        float(nx)/float(ny), 
        0.0f, 
        800.0f,
        time0, 
        time1 );
    */
    
    camera cam(
        vec3( 0.0f,  0.1f, 1.0f ), 
        vec3( 0.0f, 17.0f, 0.0f ), 
        vec3( 0.0f,  1.0f, 0.0f ), 
        35.0f, 
        float(o.nx)/float(o.ny), 
        0.0f, 
        800.0f,
        time0, 
        time1 );
    
    hitable *world = another_simple();
    bvh_node *bvh_root = new bvh_node(
        ((hitable_list*)world)->list,
        ((hitable_list*)world)->list_size,
        time0, time1 );
    
    // percent compute
    int nb_pixels = o.nx * o.ny;
    int pixels_per_percent = nb_pixels / 100;
    int current_nb_pixels = 0;
    int percent = 0;
    
    // tile setup
    int tile_width = TILE_WIDTH;
    int tile_height = TILE_HEIGHT;
    int nb_tile_x = o.nx / tile_width;
    int nb_tile_y = o.ny / tile_height;
    int remainder_x = o.nx - ( nb_tile_x * tile_width );
    int remainder_y = o.ny - ( nb_tile_y * tile_height );
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
            task->image_width = o.nx;
            task->image_height = o.ny;
            task->samples = o.ns;
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
        o.out_filename.c_str(),
        o.nx, o.ny, 4,
        (void*)image_buffer,
        4*o.nx); // row stride
    
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

hitable *another_simple()
{
    int l = 0;
    hitable **list = new hitable*[100];
    
    // sky
    list[l++] = new flip_normals( new sphere(vec3(0,0,0), 50, new diffuse_light(new constant_texture(vec3(1,1,1)))));
    
    // giant floor
    list[l++] = new sphere(vec3(0,-1000,0), 1000, new lambertian(new constant_texture(vec3(0.5,0.5,0.5))));
    
    // sphere
    const float sphere_height = 17.0f;
    const float sphere_radius = 1.0f;
    const float diameter = 2.0f * sphere_radius;
    const int nb = 5;
    const float half_scene_size = ( nb * diameter ) / 2.0f;
    for ( int i = 0; i < nb; ++i )
    {
        for ( int j = 0; j < nb; ++j )
        {
            vec3 position(
                i * diameter - half_scene_size + sphere_radius, 
                sphere_height, 
                j * diameter - half_scene_size + sphere_radius);
            float density = (nb-j)*1.0f;
            float ior = 1.0f + (i+1)*0.1f;
            hitable *boundary = new sphere(position, 1.0f, new dielectric(ior));
            list[l++] = boundary;
            list[l++] = new constant_medium(boundary, density, new constant_texture(vec3(0.2f, 0.4f, 0.9f)));
        }
    }
    return new hitable_list(list, l);
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
    
    list[i++] = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 165, 165), white), -18 ), vec3( 130, 0,  65 ));  // small box
    list[i++] = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 330, 165), white),  15 ), vec3( 265, 0, 295 )); // big box
    
    return new hitable_list(list, i);
}

hitable *cornell_box_volumes()
{
    hitable **list = new hitable*[8];
    int i = 0;
    material *red   = new lambertian( new constant_texture(vec3(0.65f,0.05f,0.05f)));
    material *white = new lambertian( new constant_texture(vec3(0.73f,0.73f,0.73f)));
    material *green = new lambertian( new constant_texture(vec3(0.12f,0.45f,0.15f)));
    material *light = new diffuse_light( new constant_texture(vec3(15,15,15)));
    texture *volume_light = new constant_texture( vec3( 1.0f, 1.0f, 1.0f ) );
    texture *volume_dark  = new constant_texture( vec3( 0.0f, 0.0f, 0.0f ) );
    
    list[i++] = new flip_normals(new yz_rect(0,555,0,555,555, green)); // left
    list[i++] = new yz_rect(0,555,0,555,  0, red);                     // right
    list[i++] = new flip_normals(new xz_rect(0,555,0,555,555, white)); // top
    list[i++] = new xz_rect(213,343,227,332,554, light);               // light
    list[i++] = new xz_rect(0,555,0,555,0, white);                     // bottom
    list[i++] = new flip_normals(new xy_rect(0,555,0,555,555, white)); // back
    
    hitable *b1 = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 165, 165), white), -18 ), vec3( 130, 0,  65 ));  // small box
    hitable *b2 = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 330, 165), white),  15 ), vec3( 265, 0, 295 )); // big box
    
    list[i++] = new constant_medium( b1, 0.02f, volume_light );
    list[i++] = new constant_medium( b2, 0.01f, volume_dark );
    
    return new hitable_list(list, i);
}

hitable *mega_big_scene_end_of_book2()
{
    hitable **list = new hitable*[30];
    hitable **boxlist = new hitable*[400];
    hitable **boxlist2 = new hitable*[1000];
    
    material *white  = new lambertian( new constant_texture(vec3(0.73f,0.73f,0.73f)));
    material *ground = new lambertian( new constant_texture(vec3(0.48f,0.83f,0.53f)));
    
    // 20x20 floor of boxes of 100x100xrandom_height
    int nb = 20;
    int b = 0; // total_nb_boxes
    for ( int i = 0; i < nb; ++i )
    {
        for ( int j = 0; j < nb; ++j )
        {
            float w = 100.0f;
            float x0 = -1000.0f + i * w;
            float z0 = -1000.0f + j * w;
            float y0 = 0.0f;
            float x1 = x0 + w;
            float y1 = 100.0f * ( RAN01() + 0.01f );
            float z1 = z0 + w;
            boxlist[b++] = new box( vec3(x0, y0, z0), vec3(x1, y1, z1), ground );
        }
    }
    
    int l = 0;
    list[l++] = new bvh_node( boxlist, b, 0.0f, 1.0f );
    
    // cornell light
    material *light = new diffuse_light( new constant_texture(vec3(7,7,7)));
    list[l++] = new xz_rect(123, 423, 147, 412, 554, light);
    
    // motion blurred sphere
    vec3 center(400,400,200);
    list[l++] = new moving_sphere( 
        center, center + vec3(30,0,0), 0, 1, 50, 
        new lambertian(
        new constant_texture(
        vec3(0.7f, 0.3f, 0.1f)
        )
        )
        );
    
    // glass sphere
    list[l++] = new sphere(vec3(260, 150, 45), 50, new dielectric(1.5f));
    
    // metal sphere
    list[l++] = new sphere(vec3(0, 150, 145), 50, new metal(new constant_texture(vec3(0.8f, 0.8f, 0.9f)), 10.0f));
    
    // subsurface sphere 1
    hitable *boundary = new sphere(vec3(360, 150, 145), 70, new dielectric(1.5f));
    list[l++] = boundary;
    list[l++] = new constant_medium(boundary, 0.2f, new constant_texture(vec3(0.2f, 0.4f, 0.9f)));
    
    // fog over all scene
    boundary = new sphere(vec3(0,0,0),5000, new dielectric(1.5f));
    list[l++] = new constant_medium(boundary, 0.0001f, new constant_texture(vec3(1.0f, 1.0f, 1.0f)));
    
    // textured sphere
    int nx, ny, nn;
    unsigned char *tex_data = stbi_load("../data/earth.jpg", &nx, &ny, &nn, 0);
    material *emat = new lambertian(new image_texture(tex_data, nx, ny));
    list[l++] = new sphere(vec3(400,200,400), 100, emat);
    
    // noise sphere
    texture *perlin_tex = new noise_texture(0.1f);
    list[l++] = new sphere(vec3(220, 280, 300), 80, new lambertian(perlin_tex));
    
    // cube of many spheres
    int ns = 1000;
    for ( int j = 0; j < ns; ++j )
    {
        boxlist2[j] = new sphere(vec3(165.0f*RAN01(),165.0f*RAN01(),165.0f*RAN01()), 10, white );
    }
    list[l++] = new translate( new rotate_y( new bvh_node(boxlist2, ns, 0.0f, 1.0f), 15.0f), vec3(-100.0f,270.0f,395.0f));
    
    
    return new hitable_list(list, l);
}

// eof
