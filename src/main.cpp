#include <iostream>
#include <fstream>
#include <cmath>
#include <cstdlib>
#include <float.h>
#include <random>
#include <chrono>
#include <iomanip> // set::setprecision

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

#define OUT_WIDTH 200
#define OUT_HEIGHT 100
#define NB_SAMPLES 300 // samples per pixel for AA
#define RECURSE_DEPTH 50

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
    int n = 50;//500
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
    
    int nb_pixels = nx * ny;
    int pixels_per_percent = nb_pixels / 100;
    int current_nb_pixels = 0;
    int percent = 0;
    
    auto time_start = std::chrono::high_resolution_clock::now();
    
    for ( int j = ny-1; j >=0; --j ) // top to bottom
    {
        for ( int i = 0; i < nx; ++i ) // left to right
        {
            vec3 col(0,0,0);
            for ( int s = 0; s < ns; ++s ) // multisample
            {
                float u = float(i+RAN01()) / float(nx);
                float v = float(j+RAN01()) / float(ny);
                
                ray r = cam.get_ray(u,v);
                col += color(r, world, 0);
            }
            // resolve AA
            col /= (float)ns;
            
            // gamma correction
            col = vec3(sqrtf(col[0]), sqrtf(col[1]), sqrtf(col[2]) );
            unsigned int ir = unsigned int(255.99f*col.r());
            unsigned int ig = unsigned int(255.99f*col.g());
            unsigned int ib = unsigned int(255.99f*col.b());
            
            // 0xABGR
            *buffer_ptr++ = ( 0xff000000 | (ib << 16) | (ig << 8) | (ir << 0) );
            
            if ( (++current_nb_pixels) % pixels_per_percent == 0 )
            {
                std::cout << "Generating... " << ++percent << "%\r";
            }
        }
    }
    
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
