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
 
 void get_sphere_uv( const vec3 &p, float &u, float &v )
 {
     float phi = atan2( p.z(), p.x() );   // [-PI..PI]
     float theta = asin( p.y() );         // [-PI/2..PI/2]
     float two_pi = (2.0f * (float)M_PI);
     float pi_over_two = ((float)M_PI / 2.0f);
     float phi_minus_pi = (phi + (float)M_PI); // [-2PI..0]
     float phi_minus_pi_over_two_pi = ( phi_minus_pi / two_pi ); // [-1..0]
     u = 1.0f - phi_minus_pi_over_two_pi; // [-PI..PI] -> [0..1]
     v = (theta + pi_over_two) / (float)M_PI;        // [-PI/2..PI/2] -> [0..1]
     //std::cout << "u:"<< u << " v:" << v << "\n";
 }
 
#endif // _RAYTRACER_UTILS_H_
 