 #ifndef _RAYTRACER_RAY_H_
#define _RAYTRACER_RAY_H_
 
 struct ray
 {
     ray(){}
     ray(const vec3 &a, const vec3 &b) : A(a), B(b) {}
     vec3 origin() const { return A;}
     vec3 direction() const { return B;}
     vec3 point_at_parameter(float t) const { return A + t*B; }
     
     vec3 A;
     vec3 B;
 };
 
#endif // _RAYTRACER_RAY_H_
 
 
 
