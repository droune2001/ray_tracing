 #ifndef _RAYTRACER_CAMERA_H_
#define _RAYTRACER_CAMERA_H_
 
 struct camera
 {
     // vfov in degrees, full angle from top to bottom
     camera(vec3 eye, vec3 lookat, vec3 up, 
            float vfov, float aspect,
            float aperture, float focus_dist,
            float t0, float t1 );
     inline ray get_ray( float u, float v );
     
     // camera basis
     vec3 origin = vec3(0,0,0);
     vec3 u = vec3(1,0,0);
     vec3 v = vec3(0,1,0);
     vec3 w = vec3(0,0,1);
     
     // projection plane
     vec3 lower_left_corner = vec3( -2.0f, -1.0f, -1.0f );
     vec3 horizontal        = vec3( 4.0f, 0.0f, 0.0f );
     vec3 vertical          = vec3( 0.0f, 2.0f, 0.0f );
     
     // lens
     float lens_radius = 1.0f;
     float time0 = 0.0f, time1 = 1.0f; // shutter open/close times
 };
 
 camera::camera(vec3 eye, vec3 lookat, vec3 up, 
                float vfov, float aspect,
                float aperture, float focus_dist,
                float t0, float t1 ) 
     : origin(eye), time0(t0), time1(t1)
 {
     lens_radius = aperture / 2.0f;
     float theta = vfov * PI / 180.0f; // to radians
     float half_height = tanf( theta / 2.0f );
     float half_width = aspect * half_height;
     
     w = unit_vector( eye - lookat );
     u = unit_vector( cross( up, w ) );
     v = cross( w, u );
     // z = focus_dist plane in camera space
     lower_left_corner = 
         origin 
         - half_width * focus_dist * u 
         - half_height * focus_dist * v
         - w * focus_dist;
     // size of the proj plane is scaled by focus dist
     horizontal = 2.0f * half_width * focus_dist * u; // cam X
     vertical = 2.0f * half_height * focus_dist * v; // cam Y
 }
 
 inline ray camera::get_ray(float s, float t)
 {
     vec3 rd = lens_radius * random_in_unit_disk();
     vec3 offset = origin + rd;
     vec3 offset_origin = origin + offset;
     // gen random time for ray (motion blur)
     float time = time0 + RAN01() * ( time1 - time0 );
     // shoot a ray from a random point in the lens disk
     // that all focus on the (u,v) sample on the focal plane.
     return ray( offset_origin,
                ( lower_left_corner 
                 + s * horizontal 
                 + t * vertical ) 
                - offset_origin, time );
 }
 
#endif // _RAYTRACER_CAMERA_H_
 