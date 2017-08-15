 #ifndef _RAYTRACER_SPHERE_H_
#define _RAYTRACER_SPHERE_H_
 
 class sphere : public hitable
 {
     public:
     
     sphere() {}
     sphere(vec3 cen, float r, material *the_mat) : center(cen), radius(r), mat(the_mat) {}
     virtual bool hit(const ray &r, float t_min, float t_max, hit_record &rec ) const override;
     virtual bool bounding_box( float t0, float t1, aabb &box) const override;
     
     virtual float pdf_value( const vec3 &o, const vec3 &v ) const override;
     virtual vec3 random( const vec3 &o ) const override;
     
     material *mat = nullptr;
     vec3 center = vec3(0,0,0);
     float radius = 1.0f;
 };
 
 bool sphere::hit(const ray &r, float t_min, float t_max, hit_record &rec) const
 {
     rec.mat_ptr = mat;
     
     // NOTE(nfauvet): removed 4's and 2's that cancel each others out.
     vec3 oc = r.origin() - center;
     float a = dot( r.direction(), r.direction() );
     //float b = 2.0f * dot( oc, r.direction() );
     float b = dot( oc, r.direction() );
     float c = dot( oc, oc ) - radius * radius;
     //float discriminant = b*b - 4*a*c;
     float discriminant = b*b - a*c;
     if ( discriminant > 0.0f )
     {
         //float temp = (-b - sqrtf(discriminant)) / (2.0f*a);
         float temp = (-b - sqrtf(discriminant)) / a;
         if (temp < t_max && temp > t_min)
         {
             rec.t = temp;
             rec.p = r.point_at_parameter(rec.t);
             rec.normal = (rec.p - center) / radius;
             get_sphere_uv( rec.normal, rec.u, rec.v );
             return true;
         }
         
         
         temp = (-b + sqrtf(discriminant)) / a;
         if (temp < t_max && temp > t_min)
         {
             rec.t = temp;
             rec.p = r.point_at_parameter(rec.t);
             rec.normal = (rec.p - center) / radius;
             get_sphere_uv( rec.normal, rec.u, rec.v );
             return true;
         }
     }
     
     return false;
 }
 
 bool sphere::bounding_box( float t0, float t1, aabb &box) const
 {
     box = aabb(center - vec3(radius, radius, radius),
                center + vec3(radius, radius, radius));
     return true;
 }
 
 float sphere::pdf_value( const vec3 &o, const vec3 &v ) const
 {
     hit_record hrec = {};
     // find where the ray hit me
     if ( this->hit( ray( o, v ), 0.001f, FLT_MAX, hrec ) )
     {
         // encompassing cone of the sphere viewd from "o"
         float cos_theta_max = sqrtf( 1.0f - radius * radius / ( center - o ).squared_length() );
         float solid_angle = 2.0f * PI * ( 1.0f - cos_theta_max );
         return 1.0f / solid_angle; // hitting that sphere had that pdf_value
     }
     else
     {
         return 0.0f;
     }
 }
 
 inline vec3 random_to_sphere( float radius, float distance_squared )
 {
     float r1 = RAN01();
     float r2 = RAN01();
     float z = 1.0f + r2 * ( sqrtf( 1.0f - radius * radius / distance_squared ) - 1.0f );
     float phi = 2.0f * PI * r1;
     float x = cosf( phi ) * sqrtf( 1.0f - z * z );
     float y = sinf( phi ) * sqrtf( 1.0f - z * z );
     return vec3( x, y, z );
 }
 
 vec3 sphere::random( const vec3 &o ) const
 {
     vec3 direction = center - o;
     float distance_squared = direction.squared_length();
     onb uvw;
     uvw.build_from_w( direction );
     return uvw.local( random_to_sphere( radius, distance_squared ) );
 }
 
 
 
 
 
 
 
 
 
 
 // can derive from sphere
 class moving_sphere : public hitable
 {
     public:
     
     moving_sphere() {}
     moving_sphere( 
         vec3 cen0, vec3 cen1, 
         float t0, float t1,
         float r, material *m ) : 
     center0(cen0), center1(cen1), 
     time0(t0), time1(t1),
     radius(r), mat_ptr(m) {}
     
     virtual bool hit(const ray &r, float t_min, float t_max, hit_record &rec ) const override;
     virtual bool bounding_box( float t0, float t1, aabb &box) const override;
     
     inline vec3 center(float t) const;
     
     material *mat_ptr = nullptr;
     vec3 center0 = vec3(0,0,0);
     vec3 center1 = vec3(1,0,0);
     float time0 = 0.0f;
     float time1 = 1.0f;
     float radius = 1.0f;
 };
 
 // linear interpolation of center at time t
 inline vec3 moving_sphere::center(float t) const
 {
     return center0 + ((t-time0)/(time1-time0))*(center1-center0);
 }
 
 bool moving_sphere::hit(const ray &r, float t_min, float t_max, hit_record &rec) const
 {
     rec.mat_ptr = mat_ptr;
     
     // NOTE(nfauvet): removed 4's and 2's that cancel each others out.
     vec3 oc = r.origin() - center(r.time());
     float a = dot( r.direction(), r.direction() );
     //float b = 2.0f * dot( oc, r.direction() );
     float b = dot( oc, r.direction() );
     float c = dot( oc, oc ) - radius * radius;
     //float discriminant = b*b - 4*a*c;
     float discriminant = b*b - a*c;
     if ( discriminant > 0.0f )
     {
         //float temp = (-b - sqrtf(discriminant)) / (2.0f*a);
         float temp = (-b - sqrtf(discriminant)) / a;
         if (temp < t_max && temp > t_min)
         {
             rec.t = temp;
             rec.p = r.point_at_parameter(rec.t);
             rec.normal = (rec.p - center(r.time())) / radius;
             return true;
         }
         
         
         temp = (-b + sqrtf(discriminant)) / a;
         if (temp < t_max && temp > t_min)
         {
             rec.t = temp;
             rec.p = r.point_at_parameter(rec.t);
             rec.normal = (rec.p - center(r.time())) / radius;
             return true;
         }
     }
     
     return false;
 }
 
 bool moving_sphere::bounding_box( float t0, float t1, aabb &box) const
 {
     aabb box0 = aabb(center0 - vec3(radius, radius, radius),
                      center0 + vec3(radius, radius, radius));
     aabb box1 = aabb(center1 - vec3(radius, radius, radius),
                      center1 + vec3(radius, radius, radius));
     box = surrounding_box(box0, box1);
     return true;
 }
 
#endif // _RAYTRACER_SPHERE_H_
 
 
