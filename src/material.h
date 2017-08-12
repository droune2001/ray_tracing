 #ifndef _RAYTRACER_MATERIAL_H_
#define _RAYTRACER_MATERIAL_H_
 
 vec3 random_in_unit_sphere();
 vec3 reflect(const vec3 &v, const vec3 &n);
 bool refract(const vec3 &v, const vec3 &n, float ni_over_nt, vec3& refracted );
 float schlick(float cosine, float ref_idx);
 
 struct material 
 {
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &albedo, ray &scattered, float &pdf ) const = 0;
     virtual float scattering_pdf(const ray &r_in, const hit_record &rec, const ray &scattered) const = 0;
     virtual vec3 emitted(const ray &r_in, const hit_record &rec, float u, float v, const vec3 &p) const { return vec3(0,0,0); };
 };
 
 struct lambertian : public material
 {
     lambertian( texture *a) : albedo(a) {}
     virtual float scattering_pdf(const ray &r_in, const hit_record &rec, const ray &scattered) const
     {
         float cosine = dot(rec.normal, scattered.direction());
         if ( cosine < 0.0f) cosine = 0.0f;
         return cosine / PI;
     }
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &alb, ray &scattered, float &pdf) const
     {
         //vec3 target = rec.p + rec.normal + random_in_unit_sphere(); // TODO: scatter towards lights
         //scattered = ray(rec.p, target-rec.p, r_in.time());
         //alb = albedo->value( rec.u, rec.v, rec.p );
         //pdf = dot(rec.normal, scattered.direction()) / PI; // cos(theta)/pi
         
         // hemisphere above
         onb uvw;
         uvw.build_from_w( rec.normal );
         vec3 direction = uvw.local( random_cosine_direction() );
         scattered = ray(rec.p, unit_vector(direction), r_in.time());
         alb = albedo->value( rec.u, rec.v, rec.p );
         pdf = dot( uvw.w(), scattered.direction() ) / PI;
         
         return true;
     }
     
     texture *albedo = nullptr;
 };
 
 struct metal : public material
 {
     metal( texture *a, float f) : albedo(a) 
     { 
         if ( f < 1.0f ) 
         {
             fuzz = f; 
         }
         else 
         {
             fuzz = 1; 
         }
     }
     
     virtual float scattering_pdf(const ray &r_in, const hit_record &rec, const ray &scattered) const
     {
         return 1.0f;
     }
     
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered, float &pdf) const
     {
         // pure reflection, no random
         vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
         scattered = ray(rec.p, reflected + fuzz * random_in_unit_sphere(), r_in.time());
         attenuation = albedo->value( rec.u, rec.v, rec.p );
         
         pdf = 1.0f;
         
         // dont scatter at angles > 90 degrees
         return (dot(scattered.direction(), rec.normal) > 0.0f);
     }
     
     texture *albedo = nullptr;
     float fuzz = 1.0f;
 };
 
 struct dielectric : public material
 {
     dielectric( float ri ) : ref_idx(ri) {}
     virtual float scattering_pdf(const ray &r_in, const hit_record &rec, const ray &scattered) const
     {
         return 1.0f;
     }
     
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered, float &pdf) const
     {
         vec3 outward_normal;
         vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
         float ni_over_nt;
         attenuation = vec3(1.0f, 1.0f, 1.0f); // white glass
         vec3 refracted;
         float reflect_prob;
         float cosine;
         
         // interior or exterior??
         if (dot(r_in.direction(), rec.normal) > 0.0f)
         {
             outward_normal = -rec.normal;
             ni_over_nt = ref_idx;
             cosine = ref_idx * dot(r_in.direction(), rec.normal) / r_in.direction().length(); 
         }
         else // exterior
         {
             outward_normal = rec.normal;
             ni_over_nt = 1.0f / ref_idx;
             // dot(N,L)
             cosine = -dot(r_in.direction(), rec.normal) / r_in.direction().length(); 
         }
         
         
         if (refract(r_in.direction(), outward_normal, ni_over_nt, refracted))
         {
             reflect_prob = schlick(cosine, ref_idx);
         }
         else
         {
             reflect_prob = 1.0f;
         }
         
         // no blending yet between reflect and refract, just a proba.
         if (distribution(generator) < reflect_prob)
         {
             scattered = ray(rec.p, reflected, r_in.time());
         }
         else
         {
             scattered = ray(rec.p, refracted, r_in.time());
         }
         
         pdf = 1.0f;
         
         return true;
     }
     
     float ref_idx = 1.0f;
 };
 
 struct diffuse_light : public material
 {
     diffuse_light(texture *E) : emit(E) {}
     virtual float scattering_pdf(const ray &r_in, const hit_record &rec, const ray &scattered) const
     {
         return 1.0f;
     }
     
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered, float &pdf) const 
     { 
         pdf = 1.0f;
         return false; 
     }
     
     virtual vec3 emitted( const ray &r_in, const hit_record &rec, float u, float v, const vec3 &p) const 
     { 
         //if ( dot( rec.normal, r_in.direction() ) < 0.0f )
         //{
         return emit->value( u, v, p );
         //}
         //else
         //{
         //return vec3(0,0,0);
         //}
     }
     
     texture *emit;
 };
 
 struct isotropic : public material
 {
     isotropic( texture *a ) : albedo(a) {}
     virtual float scattering_pdf(const ray &r_in, const hit_record &rec, const ray &scattered) const
     {
         return 1.0f;
     }
     
     virtual bool scatter( const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered, float &pdf) const 
     { 
         // random scatter from the scattering point
         scattered = ray( rec.p, random_in_unit_sphere() );
         attenuation = albedo->value( rec.u, rec.v, rec.p );
         pdf = 1.0f;
         return true; 
     }
     
     texture *albedo;
 };
 
#endif // _RAYTRACER_MATERIAL_H_
 