/** raytracer
 *  @author: nfauvet
 *  @usage: main.exe -w 1920 -h 1080 -s 300 -r 50 -t 4 -o "out.png"
 
 "w,width"       "Output image width"              "1920"
 "h,height"      "Output image height"             "1080"
         "s,samples"     "Number of samples per pixel"     "300"
         "r,recurse"     "Number of bounces"               "1"       "50"
         "t,threads"     "Number of threads"               "1"
         "tw"            "Tile width"                      "1"
         "th"            "Tile height"                     "1"
         "windowed"      "Show a preview window"           "0"       "1"
         "i,input"       "Input filename"
         "o,output"      "Output filename"                 "out.png"
         "p,passes"      "Output passes as separate files" "0"       "1"
         "m,multi"       "Output one file for each sample" "0"       "1"
         "rx"            "ROI start x (from left)"         "0"
         "ry"            "ROI start y (from top)"          "0"
         "rw"            "ROI width"                       "1"
         "rh"            "ROI height"                      "1"
         "x,exit"        "Exit without rendering"          "0"       "1"
         "v,verbose"     "Prints text"                     "0"       "1"
         "extra-verbose" "Prints extra text"               "0"       "1"
         
 */

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

#define CXXOPTS_NO_RTTI
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
#include "scenes.h"

// NOTE(nfauvet): pgcd(1920,1080) = 120
// 120 = 2*2*2*3*5

vec3 color( const ray &r, hitable *world, int max_depth, int depth )
{
    hit_record rec = {};
    if (world->hit(r, 0.001f, FLT_MAX, rec))
    {
        ray scattered;
        vec3 attenuation;
        vec3 emitted = rec.mat_ptr->emitted( rec.u, rec.v, rec.p );
        if ((depth < max_depth) && rec.mat_ptr->scatter(r, rec, attenuation, scattered))
        {
            // recursive
            return emitted + attenuation * color(scattered, world, max_depth, depth+1);
        }
        else
        {
            return emitted;
        }
    }
    else
    {
        return vec3(0,0,0);
    }
}

global std::mutex g_console_mutex;
global int g_total_nb_tiles = 0;
global int g_nb_tiles_finished = 0;
global int g_nb_samples_finished = 0;
global int g_total_nb_samples = 0;
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
                    col += color(r, world, max_depth, 0);
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
    int max_depth;
    unsigned int *shared_buffer;
    hitable *world;
    camera *cam;
};

struct compute_one_sample_task : public task
{
    compute_one_sample_task() : task() {}
    
    virtual ~compute_one_sample_task() override
    {
        std::unique_lock<std::mutex> g(g_console_mutex);
        ++g_nb_samples_finished;
        g_percent_complete = 100.0f * (float)g_nb_samples_finished / (float)g_total_nb_samples;
        
        std::cout << g_nb_samples_finished << "/" << g_total_nb_samples << " : " 
            << std::fixed << std::setprecision(2) << g_percent_complete << "%\r";
    }
    
    virtual void run() override
    {
        for( int j = image_height-1; j >= 0; --j )
        {
            float *line_buffer_ptr = 
                shared_buffer + 
                (image_height - 1 - j) * 4 * image_width;
            
            for( int i = 0; i < image_width; ++i )
            {
                vec3 col( 0, 0, 0 );
                for ( int s = 0; s < sub_samples; ++s ) // multisample
                {
                    float u = float( i + RAN01() ) / float( image_width );
                    float v = float( j + RAN01() ) / float( image_height );
                    
                    ray r = cam->get_ray( u, v );
                    col += color( r, world, max_depth, 0 );
                }
                col /= (float)sub_samples;
                
                // ABGR
                *line_buffer_ptr++ = 1.0f;
                *line_buffer_ptr++ = col.b();
                *line_buffer_ptr++ = col.g();
                *line_buffer_ptr++ = col.r();
            }
        }
    }
    
    virtual void showTask() override
    {
        //std::unique_lock<std::mutex> g(g_console_mutex);
        //std::cout << "thread " << "( " << tile_origin_x << ", " << tile_origin_y << ")"
        //<< " id(" << std::this_thread::get_id() << ") working...\n";
    }
    
    int image_width = 1;
    int image_height = 1;
    int sub_samples = 1;
    int sample_id = 0;
    int max_depth;
    float *shared_buffer;
    hitable *world;
    camera *cam;
};

int main( int argc, char **argv )
{
    cxxopts::Options options( "raytracer", "nfauvet raytracer" );
    options.add_options()
        ( "w,width",       "Output image width", cxxopts::value<int>()->default_value( "1920" ) )
        ( "h,height",      "Output image height", cxxopts::value<int>()->default_value( "1080" ) )
        ( "s,samples",     "Number of samples per pixel", cxxopts::value<int>()->default_value( "256" ) )
        ( "S,sub-samples", "Number of sub-samples per thread", cxxopts::value<int>()->default_value( "8" ) )
        ( "r,recurse",     "Number of bounces", cxxopts::value<int>()->default_value( "1" )->implicit_value( "50" ) )
        ( "t,threads",     "Number of threads", cxxopts::value<int>()->default_value( "1" ) )
        ( "tw",            "Tile width", cxxopts::value<int>()->default_value( "1" ) )
        ( "th",            "Tile height", cxxopts::value<int>()->default_value( "1" ) )
        ( "windowed",      "Show a preview window", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ( "i,input",       "Input filename", cxxopts::value<std::string>() )
        ( "o,output",      "Output filename", cxxopts::value<std::string>()->default_value( "out.png" ) )
        ( "p,passes",      "Output passes as separate files", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ( "m,multi",       "Output one file for each sample", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ( "rx",            "ROI start x (from left)", cxxopts::value<int>()->default_value( "0" ) )
        ( "ry",            "ROI start y (from top)", cxxopts::value<int>()->default_value( "0" ) )
        ( "rw",            "ROI width", cxxopts::value<int>()->default_value( "1" ) )
        ( "rh",            "ROI height", cxxopts::value<int>()->default_value( "1" ) )
        ( "x,exit",        "Exit without rendering", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ( "v,verbose",     "Prints text", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ( "V,extra-verbose", "Prints extra text", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ;
    
    options.parse( argc, argv );
    
    struct 
    {
        int nx;
        int ny;
        int ns;
        int nss;
        int bounces;
        int threads;
        int windowed;
        std::string in_filename;
        std::string out_filename;
        int passes;
        int out_separate;
        int rx;
        int ry;
        int rw;
        int rh;
        int dontrender;
        int verbose;
        int extraverbose;
    } o;
    
    // parse
    o.nx = options["w"].as<int>();
    o.ny = options["h"].as<int>();
    o.ns = options["s"].as<int>();
    o.nss = options["S"].as<int>();
    o.bounces = options["r"].as<int>();
    o.threads = options["t"].as<int>();
    o.windowed = options["windowed"].as<int>();
    o.in_filename = options["i"].as<std::string>();
    o.out_filename = options["o"].as<std::string>();
    o.passes = options["p"].as<int>();
    o.out_separate = options["m"].as<int>();
    o.rx = options["rx"].as<int>();
    o.ry = options["ry"].as<int>();
    o.rw = options["rw"].as<int>();
    o.rh = options["rh"].as<int>();
    o.dontrender = options["x"].as<int>();
    o.verbose = options["v"].as<int>();
    o.extraverbose = options["extra-verbose"].as<int>();
    
    if ( o.verbose )
    {
        std::cout << std::endl;
        if ( o.extraverbose )
        {
            std::cout << "~ raytracer by nfauvet ~\n\n";
        }
        
        std::cout << "Resolution          : " << o.nx << "x" << o.ny << "\n";
        std::cout << "Samples per pixel   : " << o.ns << "\n";
        std::cout << "Samples per thread  : " << o.nss << "\n";
        std::cout << "Bounce depth        : " << o.bounces << "\n";
        std::cout << "Number of Threads   : " << o.threads << "\n";
        std::cout << "Windowed            : " << o.windowed << "\n";
        std::cout << "Input file          : \"" << o.in_filename << "\"\n";
        std::cout << "Output file         : \"" << o.out_filename << "\"\n";
        std::cout << "Render passes       : " << o.passes << "\n";
        std::cout << "One file per sample : " << o.out_separate << "\n";
        std::cout << "ROI x               : " << o.rx << "\n";
        std::cout << "ROI y               : " << o.ry << "\n";
        std::cout << "ROI width           : " << o.rw << "\n";
        std::cout << "ROI height          : " << o.rh << "\n";
        std::cout << "Exit without render : " << o.dontrender << "\n";
        std::cout << "Verbose             : " << o.verbose << "\n";
        std::cout << "Extra verbose       : " << o.extraverbose << "\n";
    }
    
    unsigned int buffer_memory_in_bytes = 0;
    
    unsigned int *image_buffer = nullptr;
    buffer_memory_in_bytes += (o.nx*o.ny)*sizeof(unsigned int);
    
    float **full_image_buffer_float = nullptr;
    buffer_memory_in_bytes += o.ns*4*o.nx*o.ny*sizeof(float);
    
    if ( o.verbose )
    {
        if ( buffer_memory_in_bytes > (1024*1024*1024) ) {
            std::cout << "Buffer memory       : " << ( buffer_memory_in_bytes / ( 1024 * 1024 * 1024 )) << " GiB\n";
        } else if ( buffer_memory_in_bytes > (1024*1024) ) {
            std::cout << "Buffer memory       : " << ( buffer_memory_in_bytes / ( 1024 * 1024 )) << " MiB\n";
        } else if ( buffer_memory_in_bytes > 1024 ) {
            std::cout << "Buffer memory       : " << ( buffer_memory_in_bytes / ( 1024 )) << " KiB\n";
        } else {
            std::cout << "Buffer memory       : " << buffer_memory_in_bytes  << " Bytes\n";
        }
    }
    
    if ( o.dontrender ) 
    {
        std::cout << "Early exit.\n";
        return 0;
    }
    
    // Allocate
    image_buffer = new unsigned int[o.nx * o.ny];
    full_image_buffer_float = new float*[o.ns];
    for ( int s = 0; s < o.ns; ++s ) 
    {
        full_image_buffer_float[s] = new float[o.nx * o.ny * 4];
    }
    
    float time0 = 0.0f;
    float time1 = 1.0f;
    
    // camera setup
    
    // cornell camera
    camera cam( vec3( 278.0f, 278.0f, -800.0f ), vec3( 278.0f, 278.0f, 278.0f ), vec3( 0.0f, 1.0f, 0.0f ), 40.0f, float(o.nx)/float(o.ny), 0.0f, 800.0f, time0, time1 );
    
    // mega book2 scene camera
    //camera cam( vec3( 350.0f, 278.0f, -450.0f ), vec3( 180.0f, 278.0f,  278.0f ), vec3( 0.0f, 1.0f, 0.0f ), 45.0f, float(nx)/float(ny), 0.0f, 800.0f, time0, time1 );
    
    /*
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
    */
    
    hitable *world = cornell_box();
    bvh_node *bvh_root = new bvh_node(
        ((hitable_list*)world)->list,
        ((hitable_list*)world)->list_size,
        time0, time1 );
    
    // percent compute
    int nb_pixels = o.nx * o.ny;
    int pixels_per_percent = nb_pixels / 100;
    int current_nb_pixels = 0;
    int percent = 0;
    
    g_percent_complete = 0;
    g_total_nb_samples = o.ns;
    g_nb_samples_finished = 0;
    
    thread_pool *pool = new thread_pool( o.threads );
    
    auto time_start = std::chrono::high_resolution_clock::now();
    
    // one task per sample
    for ( int i = 0; i < o.ns; ++i )
    {
        compute_one_sample_task *task = new compute_one_sample_task();
        task->image_width = o.nx;
        task->image_height = o.ny;
        task->sub_samples = o.nss;
        task->sample_id = i;
        task->max_depth = o.bounces;
        task->shared_buffer = full_image_buffer_float[i];
        task->world = (hitable*)bvh_root;
        task->cam = &cam;
        
        pool->addTask( task );
    }
    
    // wait
    pool->finish();
    delete pool;
    
    std::cout << "\n";
    
    // dump all sample passes
    if ( o.out_separate )
    {
        std::ostringstream oss;
        
        unsigned int *int_image_buffer = new unsigned int[o.nx*o.ny];
        
        for ( int s = 0; s < o.ns; ++s )
        {
            float *float_image = full_image_buffer_float[s];
            
            for ( int j = o.ny - 1; j >= 0; --j )
            {
                unsigned int *line_buffer_ptr = int_image_buffer + j * o.nx;
                
                for ( int i = 0; i < o.nx; ++i )
                {
                    unsigned int base_idx = j*4*o.nx+4*i;
                    float fr = float_image[ base_idx + 3 ];
                    float fg = float_image[ base_idx + 2 ];
                    float fb = float_image[ base_idx + 1 ];
                    
                    // gamma correction
                    vec3 col = vec3( sqrtf(fr), sqrtf(fg), sqrtf(fb) );
                    col.clamp01();
                    
                    unsigned int ir = (unsigned int)( col.r() * 255.99f );
                    unsigned int ig = (unsigned int)( col.g() * 255.99f );
                    unsigned int ib = (unsigned int)( col.b() * 255.99f );
                    
                    // 0xABGR
                    *line_buffer_ptr++ = ( 0xff000000 | (ib << 16) | (ig << 8) | (ir << 0) );
                }
            }
            
            oss.str("");
            oss.clear();
            std::string base_filename = o.out_filename.substr( 0, o.out_filename.find_last_of(".") );
            oss << base_filename << "_" << s << ".png";
            std::cout << "output: " << oss.str() << "\n";
            int res = stbi_write_png(
                oss.str().c_str(),
                o.nx, o.ny, 4,
                (void*)int_image_buffer,
                4*o.nx); // row stride
        }
        
        delete [] int_image_buffer;
    }
    
    // resolve float buffer
    float one_over_n_samples = 1.0f / o.ns;
    for ( int j = o.ny - 1; j >= 0; --j )
    {
        unsigned int *line_buffer_ptr = image_buffer + j * o.nx;
        
        for ( int i = 0; i < o.nx; ++i )
        {
            unsigned int base_idx = j*4*o.nx+4*i;
            float fr = 0.0f;
            float fg = 0.0f;
            float fb = 0.0f;
            for ( int s = 0; s < o.ns; ++s )
            {
                float *float_image = full_image_buffer_float[s];
                fr += float_image[ base_idx + 3 ];
                fg += float_image[ base_idx + 2 ];
                fb += float_image[ base_idx + 1 ];
            }
            
            fr *= one_over_n_samples;
            fg *= one_over_n_samples;
            fb *= one_over_n_samples;
            
            // gamma correction
            vec3 col = vec3( sqrtf(fr), sqrtf(fg), sqrtf(fb) );
            col.clamp01();
            
            unsigned int ir = (unsigned int)( col.r() * 255.99f );
            unsigned int ig = (unsigned int)( col.g() * 255.99f );
            unsigned int ib = (unsigned int)( col.b() * 255.99f );
            
            // 0xABGR
            *line_buffer_ptr++ = ( 0xff000000 | (ib << 16) | (ig << 8) | (ir << 0) );
        }
    }
    
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

// eof
