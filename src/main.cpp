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

int main( int argc, char **argv )
{
    cxxopts::Options options( "raytracer", "nfauvet raytracer" );
    options.add_options()
        ( "w,width",       "Output image width", cxxopts::value<int>()->default_value( "1920" ) )
        ( "h,height",      "Output image height", cxxopts::value<int>()->default_value( "1080" ) )
        ( "s,samples",     "Number of samples per pixel", cxxopts::value<int>()->default_value( "300" ) )
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
        ( "extra-verbose", "Prints extra text", cxxopts::value<int>()->default_value( "0" )->implicit_value( "1" ) )
        ;
    
    options.parse( argc, argv );
    
    struct 
    {
        int nx;
        int ny;
        int ns;
        int bounces;
        int threads;
        int tile_width;
        int tile_height;
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
    o.bounces = options["r"].as<int>();
    o.threads = options["t"].as<int>();
    o.tile_width = options["tw"].as<int>();
    o.tile_height = options["th"].as<int>();
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
        std::cout << "Bounce depth        : " << o.bounces << "\n";
        std::cout << "Number of Threads   : " << o.threads << "\n";
        std::cout << "Tile width          : " << o.tile_width << "\n";
        std::cout << "Tile height         : " << o.tile_height << "\n";
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
    
    if ( options.count("x") ) 
    {
        return 0;
    }
    
    
    
    
    unsigned int *image_buffer = new unsigned int[o.nx * o.ny];
    unsigned int *buffer_ptr = image_buffer;
    
    float time0 = 0.0f;
    float time1 = 1.0f;
    
    // camera setup
    
    // cornell camera
    //camera cam( vec3( 278.0f, 278.0f, -800.0f ), vec3( 278.0f, 278.0f,  278.0f ), vec3( 0.0f, 1.0f, 0.0f ), 40.0f, float(nx)/float(ny), 0.0f, 800.0f, time0, time1 );
    
    // mega book2 scene camera
    //camera cam( vec3( 350.0f, 278.0f, -450.0f ), vec3( 180.0f, 278.0f,  278.0f ), vec3( 0.0f, 1.0f, 0.0f ), 45.0f, float(nx)/float(ny), 0.0f, 800.0f, time0, time1 );
    
    
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
    int tile_width = o.tile_width;
    int tile_height = o.tile_height;
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
    
    thread_pool *pool = new thread_pool( o.threads );
    
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
            task->max_depth = o.bounces;
            task->shared_buffer = image_buffer;
            task->world = (hitable*)bvh_root;//world;
            task->cam = &cam;
            
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

// eof
