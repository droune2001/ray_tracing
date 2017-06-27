 #ifndef _RAYTRACER_MATERIAL_H_
#define _RAYTRACER_MATERIAL_H_
 
 vec3 random_in_unit_sphere();
 vec3 reflect(const vec3 &v, const vec3 &n);
 bool refract(const vec3 &v, const vec3 &n, float ni_over_nt, vec3& refracted );
 float schlick(float cosine, float ref_idx);
 
 struct material 
 {
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered ) const = 0;
 };
 
 struct lambertian : public material
 {
     lambertian( texture *a) : albedo(a) {}
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered) const
     {
         vec3 target = rec.p + rec.normal + random_in_unit_sphere();
         scattered = ray(rec.p, target-rec.p, r_in.time());
         attenuation = albedo->value(0.0f, 0.0f, rec.p);
         // NOTE(nfauvet): we could also only scatter with some probability p
         // and have attenuation be albedo/p
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
     
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered) const
     {
         // pure reflection, no random
         vec3 reflected = reflect(unit_vector(r_in.direction()), rec.normal);
         scattered = ray(rec.p, reflected + fuzz * random_in_unit_sphere(), r_in.time());
         attenuation = albedo->value(0.0f, 0.0f, rec.p);
         
         // dont scatter at angles > 90 degrees
         return (dot(scattered.direction(), rec.normal) > 0.0f);
     }
     
     texture *albedo = nullptr;
     float fuzz = 1.0f;
 };
 
 struct dielectric : public material
 {
     dielectric( float ri ) : ref_idx(ri) {}
     virtual bool scatter(const ray &r_in, const hit_record &rec, vec3 &attenuation, ray &scattered) const
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
         
         return true;
     }
     
     float ref_idx = 1.0f;
 };
 
#endif // _RAYTRACER_MATERIAL_H_
 