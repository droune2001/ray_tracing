 #ifndef _RAYTRACER_PLANES_H_
#define _RAYTRACER_PLANES_H_
 
 struct xy_rect : public hitable
 {
     xy_rect() {}
     xy_rect(float _x0, float _x1, float _y0, float _y1, float _z, material *mat) :
     x0(_x0), x1(_x1), y0(_y0), y1(_y1), z(_z), mat_ptr(mat) {}
     virtual bool hit(const ray &r, float t0, float t1, hit_record &rec) const override
     {
         float t = (z - r.origin().z()) / r.direction().z();
         if ( t < t0 || t > t1 ) // pas sur de comprendre la...
         {
             return false;
         }
         float x = r.origin().x() + t * r.direction().x();
         float y = r.origin().y() + t * r.direction().y();
         if ( x < x0 || x > x1 || y < y0 || y > y1 )
         {
             return false;
         }
         rec.u = (x-x0)/(x1-x0);
         rec.v = (y-y0)/(y1-y0);
         rec.t = t;
         rec.mat_ptr = mat_ptr;
         rec.p = r.point_at_parameter(t);
         rec.normal = vec3(0,0,1);
         return true;
     }
     
     virtual bool bounding_box(float t0, float t1, aabb &box) const override
     {
         // small thickness
         box = aabb(vec3(x0,y0,z-0.0001f), vec3(x1,y1,z+0.0001f));
         return true;
     }
     
     material *mat_ptr;
     float x0, x1, y0, y1, z;
 };
 
 struct xz_rect : public hitable
 {
     xz_rect() {}
     xz_rect(float _x0, float _x1, float _z0, float _z1, float _y, material *mat) :
     x0(_x0), x1(_x1), z0(_z0), z1(_z1), y(_y), mat_ptr(mat) {}
     virtual bool hit(const ray &r, float t0, float t1, hit_record &rec) const override
     {
         float t = (y - r.origin().y()) / r.direction().y();
         if ( t < t0 || t > t1 ) // pas sur de comprendre la...
         {
             return false;
         }
         float x = r.origin().x() + t * r.direction().x();
         float z = r.origin().z() + t * r.direction().z();
         if ( x < x0 || x > x1 || z < z0 || z > z1 )
         {
             return false;
         }
         rec.u = (x-x0)/(x1-x0);
         rec.v = (z-z0)/(z1-z0);
         rec.t = t;
         rec.mat_ptr = mat_ptr;
         rec.p = r.point_at_parameter(t);
         rec.normal = vec3(0,1,0);
         return true;
     }
     
     virtual bool bounding_box(float t0, float t1, aabb &box) const override
     {
         // small thickness
         box = aabb(vec3(x0,y-0.0001f,z0), vec3(x1,y+0.0001f,z1));
         return true;
     }
     
     virtual float pdf_value( const vec3 &o, const vec3 &v ) const 
     { 
         hit_record rec;
         // si le rayon que tu m'as file me touche bien
         if ( this->hit( ray(o,v), 0.0001f, FLT_MAX, rec) )
         {
             float area = (x1-x0)*(z1-z0);
             float distance_squared = rec.t * rec.t * v.squared_length();
             float cosine = fabsf( dot( v, rec.normal ) / v.length() );
             return distance_squared / ( cosine * area );
         }
         else
         {
             return 0.0f; 
         }
     }
     
     // la light genere un point random uniform sur elle meme
     virtual vec3 random( const vec3 &o ) const
     { 
         vec3 random_point = vec3(x0 + RAN01()*(x1-x0), y, z0 + RAN01()*(z1-z0));
         return random_point - o;
     }
     
     material *mat_ptr;
     float x0, x1, z0, z1, y;
 };
 
 struct yz_rect : public hitable
 {
     yz_rect() {}
     yz_rect(float _y0, float _y1, float _z0, float _z1, float _x, material *mat) :
     y0(_y0), y1(_y1), z0(_z0), z1(_z1), x(_x), mat_ptr(mat) {}
     virtual bool hit(const ray &r, float t0, float t1, hit_record &rec) const override
     {
         float t = (x - r.origin().x()) / r.direction().x();
         if ( t < t0 || t > t1 ) // pas sur de comprendre la...
         {
             return false;
         }
         float z = r.origin().z() + t * r.direction().z();
         float y = r.origin().y() + t * r.direction().y();
         if ( z < z0 || z > z1 || y < y0 || y > y1 )
         {
             return false;
         }
         rec.u = (y-y0)/(y1-y0);
         rec.v = (z-z0)/(z1-z0);
         rec.t = t;
         rec.mat_ptr = mat_ptr;
         rec.p = r.point_at_parameter(t);
         rec.normal = vec3(1,0,0);
         return true;
     }
     
     virtual bool bounding_box(float t0, float t1, aabb &box) const override
     {
         // small thickness
         box = aabb(vec3(x-0.0001f,y0,z0), vec3(x+0.0001f,y1,z1));
         return true;
     }
     
     material *mat_ptr;
     float z0, z1, y0, y1, x;
 };
 
#endif // _RAYTRACER_PLANES_H_
 