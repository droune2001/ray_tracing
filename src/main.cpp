#include <iostream>
#include <fstream>
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

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
#include "stb_image_write.h"

#ifndef PI
#    define PI 3.14159f
#endif
std::default_random_engine generator;
std::uniform_real_distribution<float> distribution(0.0f,1.0f);
#define RAN01() distribution(generator)

#include "vec3.h"
#include "ray.h"
#include "utils.h"
#include "camera.h"
#include "hitable.h"
#include "hitable_list.h"
#include "material.h"
#include "sphere.h"
#include "thread_pool.h"

// NOTE(nfauvet): pgcd(1920,1080) = 120
#define OUT_WIDTH 1920
#define OUT_HEIGHT 1080
#define NB_SAMPLES 300 // samples per pixel for AA
#define RECURSE_DEPTH 50
#define TILE_WIDTH 60
#define TILE_HEIGHT 12
#define NB_THREADS 8

vec3 color( const ray &r, hitable *world, int depth )
{
    hit_record rec;
    if (world->hit(r, 0.001f, FLT_MAX, rec))
    {
        ray scattered;
        vec3 attenuation;
        if ((depth < RECURSE_DEPTH) && rec.mat_ptr->scatter(r, rec, attenuation, scattered))
        {
            // recursive
            return attenuation * color(scattered, world, depth+1);
        }
        else
        {
            // why 0?? it will render all black when we unroll
            // the recursive calls.
            return vec3(0,0,0);
        }
    }
    else
    {
        // BACKGROUND COLOR
        vec3 unit_direction = unit_vector(r.direction());
        // Y from [-1;1] to [0;1]
        float t = 0.5f * (unit_direction.y() + 1.0f);
        // gradient from blue to white
        return (1.0f - t) * vec3(1.0f, 1.0f, 1.0f) + t * vec3(0.5f, 0.7f, 1.0f);
    }
}

hitable *random_scene()
{
    int n = 500;//500
    int surf_radius = ((int)sqrtf((float)n)) / 2 - 1;
    
    hitable **list = new hitable*[n+1];
    list[0] = new sphere(vec3(0,-1000,0), 1000, new lambertian(vec3(0.5,0.5,0.5)));
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
                    list[i++] = new sphere( 
                        center, 0.2f,
                        new lambertian( vec3(RAN01()*RAN01(),
                                             RAN01()*RAN01(),
                                             RAN01()*RAN01())));
                }
                else if ( choose_mat < 0.95 ) // metal
                {
                    list[i++] = new sphere( 
                        center, 0.2f, 
                        new metal( vec3(0.5f * ( 1.0f + RAN01()), 
                                        0.5f * ( 1.0f + RAN01()), 
                                        0.5f * ( 1.0f + RAN01())),
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
    list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new lambertian(vec3(0.4f, 0.2f, 0.1f)));
    list[i++] = new sphere(vec3(4.0f,1.0f,0.0f),1.0f, new metal(vec3(0.7f, 0.6f, 0.5f), 0.0f));
    
    return new hitable_list(list, i);
}

static std::mutex g_console_mutex;
static int g_total_nb_tiles = 0;
static int g_nb_tiles_finished = 0;
static float g_percent_complete = 0.0f;

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
        //std::cout << "x - delete thread " << std::this_thread::get_id() << "\n";
        std::cout << std::fixed << std::setprecision(2) 
            << g_percent_complete << "%\r";
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
        std::unique_lock<std::mutex> g(g_console_mutex);
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
    
    hitable *world = random_scene();
    
    // camera setup
    vec3 eye = vec3( 8.0f, 1.5f, 3.0f );
    vec3 lookat = vec3( 0.0f, 0.0f, 0.0f );
    vec3 up = vec3(0,1,0);
    float dist_to_focus = (eye-lookat).length();
    float aperture = 0.5f;//2.0f;
    camera cam(eye, lookat, up, 
               35.0f, float(nx) / float(ny),
               aperture, dist_to_focus);
    
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
    for ( int j = ny-1; j >=0; j -= tile_height ) // top to bottom
    {
        for ( int i = 0; i < nx; i += tile_width ) // left to right
        {
            compute_tile_task *task = new compute_tile_task();
            task->tile_origin_x = i;
            task->tile_origin_y = j - tile_height+1; // bottom left corner of a tile
            task->tile_width = tile_width;
            task->tile_height = tile_height;
            task->image_width = nx;
            task->image_height = ny;
            task->samples = ns;
            task->shared_buffer = image_buffer;
            task->world = world;
            task->cam = &cam;
            
            {
                std::unique_lock<std::mutex> g(g_console_mutex);
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
