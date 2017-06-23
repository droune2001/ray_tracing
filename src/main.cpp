#include <iostream>
#include <cmath>
#include <cstdlib>
#include <float.h>
#include <random>

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

#define OUT_WIDTH 1920;
#define OUT_HEIGHT 1080;
#define NB_SAMPLES 300; // samples per pixel for AA

vec3 color( const ray &r, hitable *world, int depth )
{
    hit_record rec;
    if (world->hit(r, 0.001f, FLT_MAX, rec))
    {
        ray scattered;
        vec3 attenuation;
        if ((depth < 50) && rec.mat_ptr->scatter(r, rec, attenuation, scattered))
        {
            // recursive
            return attenuation * color(scattered, world, depth+1);
        }
        else
        {
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

int main( int argc, char **argv )
{
    int nx = OUT_WIDTH;
    int ny = OUT_HEIGHT;
    int ns = NB_SAMPLES;
    
    std::cout << "P3\n" << nx << " " << ny << "\n255\n";
    
    hitable *list[6];
    list[0] = new sphere(vec3(0,0,-1), 0.5f, new lambertian(vec3(0.8f, 0.3f, 0.3f)));
    list[1] =new sphere(vec3(0,-100.5f,-1), 100.0f, new lambertian(vec3(0.8f, 0.8f, 0.0f)));
    list[2] = new sphere(vec3(1,0,-1), 0.5f, new metal(vec3(0.8f, 0.6f, 0.2f), 0.3f));
    list[3] = new sphere(vec3(-1,0,-1), 0.5f, new dielectric(1.5f));
    list[4] = new sphere(vec3(-1,0,-1), -0.45f, new dielectric(1.5f));
    list[5] = new sphere(vec3(-0.3f,-0.05f,-0.5f), 0.15f, new dielectric(2.5f));
    
    hitable *world = new hitable_list(list, 6);
    
    // camera setup
    vec3 eye = vec3(-2,2,1);
    vec3 lookat = vec3(0,0,-1);
    vec3 up = vec3(0,1,0);
    float dist_to_focus = (eye-lookat).length();
    float aperture = 2.0f;
    camera cam(eye, lookat, up, 
               30.0f, float(nx) / float(ny),
               aperture, dist_to_focus);
    
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
            int ir = int(255.99f*col.r());
            int ig = int(255.99f*col.g());
            int ib = int(255.99f*col.b());
            std::cout << ir << " " << ig << " " << ib << "\n";
        }
    }
    
    delete list[0];
    delete list[1];
    
    return 0;
}
