 #ifndef _RAYTRACER_UTILS_H_
#define _RAYTRACER_UTILS_H_
 
 float schlick(float cosine, float ref_idx)
 {
     float r0 = (1-ref_idx) / (1+ref_idx);
     r0 = r0 * r0;
     return r0 + (1-r0)*powf((1-cosine), 5);
 }
 
 vec3 reflect(const vec3 &v, const vec3 &n)
 {
     return v - 2.0f * dot(v,n)*n;
 }
 
 bool refract(const vec3 &v, const vec3 &n, float ni_over_nt, vec3& refracted )
 {
     vec3 uv = unit_vector(v);
     float dt = dot(uv, n);
     float discriminant = 1.0f - ni_over_nt*ni_over_nt*(1.0f-dt*dt);
     if (discriminant > 0.0f)
     {
         refracted = ni_over_nt*(uv-n*dt) - n*sqrtf(discriminant);
         return true;
     }
     else
     {
         return false;
     }
 }
 
 vec3 random_in_unit_sphere()
 {
     vec3 p;
     do
     {
         // gen a point in unit cube -1;1
         p = 2.0f * vec3( RAN01(), RAN01(), RAN01() ) - vec3( 1.0f, 1.0f, 1.0f );
     } while( p.squared_length() >= 1.0f );
     
     return p;
 }
 
 vec3 random_in_unit_disk()
 {
     vec3 p;
     
     do
     {
         // gen a point in unit disk of radius = 1
         p = 2.0f * vec3( RAN01(), RAN01(), 0.0f ) - vec3( 1.0f, 1.0f, 0.0f );
     } while( dot(p,p) >= 1.0f );
     
     return p;
 }
 
#endif // _RAYTRACER_UTILS_H_
 