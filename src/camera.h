 #ifndef _RAYTRACER_CAMERA_H_
#define _RAYTRACER_CAMERA_H_
 
 struct camera
 {
     camera() {}
     inline ray get_ray( float u, float v );
     
     // screen is a 4/2 ratio rect at z = -1. Camera is at 0,0,0.
     vec3 origin            = vec3( 0.0f, 0.0f, 0.0f );
     vec3 lower_left_corner = vec3( -2.0f, -1.0f, -1.0f );
     vec3 horizontal        = vec3( 4.0f, 0.0f, 0.0f );
     vec3 vertical          = vec3( 0.0f, 2.0f, 0.0f );
 };
 
 inline ray camera::get_ray(float u, float v)
 {
     return ray( origin, lower_left_corner + u * horizontal + v * vertical );
 }
 
#endif // _RAYTRACER_CAMERA_H_
 