 #ifndef _RAYTRACER_PDF_H_
#define _RAYTRACER_PDF_H_
 
 struct pdf
 {
     virtual float value( const vec3 &direction ) const = 0;
     virtual vec3 generate() const = 0;
 };
 
 struct cosine_pdf : public pdf
 {
     cosine_pdf( const vec3 &w ) { uvw.build_from_w(w); }
     virtual float value( const vec3 &direction ) const
     {
         float cosine = dot(unit_vector(direction), uvw.w());
         if ( cosine > 0.0f )
         {
             return cosine / PI;
         }
         else
         {
             return 0.0f;
         }
     }
     virtual vec3 generate() const
     {
         return unit_vector( uvw.local( random_cosine_direction() ) );
     }
     
     onb uvw;
 };
 
 struct hitable_pdf : public pdf
 {
     hitable_pdf( hitable *p, const vec3 &origin) : ptr(p), o(origin){}
     virtual float value( const vec3 &direction ) const
     {
         return ptr->pdf_value( o, direction );
     }
     
     virtual vec3 generate() const
     {
         return unit_vector( ptr->random(o) );
     }
     
     vec3 o;
     hitable *ptr;
 };
 
 struct mixture_pdf : public pdf
 {
     mixture_pdf( pdf *p0, pdf *p1 ) { p[0] = p0; p[1] = p1; }
     virtual float value( const vec3 &direction ) const
     {
         return 0.5f * p[0]->value( direction ) +
             0.5f * p[1]->value( direction );
     }
     
     virtual vec3 generate() const
     {
         if ( RAN01() < 0.5f )
         {
             return p[0]->generate();
         }
         else
         {
             return p[1]->generate();
         }
     }
     
     
     
     pdf *p[2];
 };
 
#endif // _RAYTRACER_PDF_H_
 