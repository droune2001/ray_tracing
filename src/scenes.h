#ifndef _RAYTRACER_SCENES_H
#define _RAYTRACER_SCENES_H
 
hitable *mega_big_scene_end_of_book1();
hitable *mega_big_scene_end_of_book2();
hitable *simple_scene();
hitable *another_simple();
hitable *two_perlin_spheres();
hitable *cornell_box();
hitable *cornell_box_volumes();

hitable *mega_big_scene_end_of_book1()
{
    int n = 500;
    int surf_radius = ((int)sqrtf((float)n)) / 2 - 1;
    
    hitable **list = new hitable*[n+1];
    list[0] = new sphere(vec3(0,-1000,0), 1000, new lambertian(new constant_texture(vec3(0.5,0.5,0.5))));
    int i = 1;
    for( int a = -surf_radius; a < surf_radius; ++a )
    {
        for( int b = -surf_radius; b < surf_radius; ++b )
        {
            float choose_mat = RAN01();
            vec3 center( a + 0.9f * RAN01(), 0.2f, b + 0.9f * RAN01() );
            // keep only spheres outside the middle where we have the big ones
            if ( (center - vec3(4.0f, 0.2f, 0.0f)).length() > 0.9f )
            {
                if ( choose_mat < 0.8f ) // diffuse
                {
                    list[i++] = new moving_sphere(
                        center,center + vec3(0.0f, 0.5f*RAN01(), 0.0f),
                        0.0f, 1.0f,
                        0.2f,
                        new lambertian( new constant_texture(vec3(RAN01()*RAN01(),
                                                                  RAN01()*RAN01(),
                                                                  RAN01()*RAN01()))));
                }
                else if ( choose_mat < 0.95 ) // metal
                {
                    list[i++] = new sphere(
                        center, 0.2f,
                        new metal( new constant_texture( vec3(0.5f * ( 1.0f + RAN01()),
                                                              0.5f * ( 1.0f + RAN01()),
                                                              0.5f * ( 1.0f + RAN01()))),
                                  0.5f * RAN01()));
                }
                else // glass
                {
                    list[i++] = new sphere(center, 0.2f, new dielectric(1.5f));
                }
            }
        }
    }
    
    list[i++] = new sphere(vec3(0.0f,1.0f,0.0f), 1.0f, new dielectric(1.5f));
    list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new lambertian(new constant_texture(vec3(0.4f, 0.2f, 0.1f))));
    list[i++] = new sphere(vec3(4.0f,1.0f,0.0f),1.0f, new metal(new constant_texture(vec3(0.7f, 0.6f, 0.5f)), 0.0f));
    
    return new hitable_list(list, i);
    
}


hitable *simple_scene()
{
    int nx, ny, nn;
    unsigned char *tex_data = stbi_load("../data/earth.jpg", &nx, &ny, &nn, 0);
    material *textured_lambert_mat = new lambertian( new image_texture(tex_data, nx, ny));
    
    hitable **list = new hitable*[10];
    int i = 0;
    list[i++] = new sphere(vec3(0,-1000,0), 1000, new lambertian(new constant_texture(vec3(0.5,0.5,0.5))));
    list[i++] = new sphere(vec3(4.0f,1.0f,0.0f), 1.0f, new dielectric(1.5f));
    list[i++] = new sphere(vec3(0.0f,1.0f,0.0f), 1.0f, textured_lambert_mat );
    //list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new lambertian( new checker_texture(new constant_texture( vec3(0.4f, 0.2f, 0.1f)), new constant_texture( vec3(0.1f, 0.4f, 0.1f)))));
    list[i++] = new sphere(vec3(-4.0f,1.0f,0.0f),1.0f, new metal(new constant_texture(vec3(0.7f, 0.6f, 0.5f)), 0.0f));
    list[i++] = new sphere(vec3(0, 6, 0), 3, new diffuse_light(new constant_texture(vec3(4,4,3))));
    list[i++] = new xy_rect(3, 5, 1, 3, -2, new diffuse_light(new constant_texture(vec3(4,4,4))));
    return new hitable_list(list, i);
}

hitable *another_simple()
{
    int l = 0;
    hitable **list = new hitable*[100];
    
    // sky
    list[l++] = new flip_normals( new sphere(vec3(0,0,0), 50, new diffuse_light(new constant_texture(vec3(1,1,1)))));
    
    // giant floor
    list[l++] = new sphere(vec3(0,-1000,0), 1000, new lambertian(new constant_texture(vec3(0.5,0.5,0.5))));
    
    // sphere
    const float sphere_height = 17.0f;
    const float sphere_radius = 1.0f;
    const float diameter = 2.0f * sphere_radius;
    const int nb = 5;
    const float half_scene_size = ( nb * diameter ) / 2.0f;
    for ( int i = 0; i < nb; ++i )
    {
        for ( int j = 0; j < nb; ++j )
        {
            vec3 position(
                i * diameter - half_scene_size + sphere_radius, 
                sphere_height, 
                j * diameter - half_scene_size + sphere_radius);
            float density = (nb-j)*1.0f;
            float ior = 1.0f + (i+1)*0.1f;
            hitable *boundary = new sphere(position, 1.0f, new dielectric(ior));
            list[l++] = boundary;
            list[l++] = new constant_medium(boundary, density, new constant_texture(vec3(0.2f, 0.4f, 0.9f)));
        }
    }
    return new hitable_list(list, l);
}

hitable *two_perlin_spheres()
{
    texture *pertex = new noise_texture(2.0f);
    hitable **list = new hitable*[2];
    list[0] = new sphere(vec3(0,-1000,0), 1000, new lambertian( pertex ) );
    list[1] = new sphere(vec3(0.0f,2.0f,0.0f), 2.0f, new lambertian( pertex ) );
    
    return new hitable_list(list, 2);
}

hitable *cornell_box()
{
    hitable **list = new hitable*[8];
    int i = 0;
    material *red   = new lambertian( new constant_texture(vec3(0.65f,0.05f,0.05f)));
    material *white = new lambertian( new constant_texture(vec3(0.73f,0.73f,0.73f)));
    material *green = new lambertian( new constant_texture(vec3(0.12f,0.45f,0.15f)));
    material *light = new diffuse_light( new constant_texture(vec3(15,15,15)));
    
    list[i++] = new flip_normals(new yz_rect(0,555,0,555,555, green)); // left
    list[i++] = new yz_rect(0,555,0,555,  0, red);                     // right
    list[i++] = new flip_normals(new xz_rect(0,555,0,555,555, white)); // top
    list[i++] = new xz_rect(213,343,227,332,554, light);               // light
    list[i++] = new xz_rect(0,555,0,555,0, white);                     // bottom
    list[i++] = new flip_normals(new xy_rect(0,555,0,555,555, white)); // back
    
    list[i++] = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 165, 165), white), -18 ), vec3( 130, 0,  65 ));  // small box
    list[i++] = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 330, 165), white),  15 ), vec3( 265, 0, 295 )); // big box
    
    return new hitable_list(list, i);
}

hitable *cornell_box_volumes()
{
    hitable **list = new hitable*[8];
    int i = 0;
    material *red   = new lambertian( new constant_texture(vec3(0.65f,0.05f,0.05f)));
    material *white = new lambertian( new constant_texture(vec3(0.73f,0.73f,0.73f)));
    material *green = new lambertian( new constant_texture(vec3(0.12f,0.45f,0.15f)));
    material *light = new diffuse_light( new constant_texture(vec3(15,15,15)));
    texture *volume_light = new constant_texture( vec3( 1.0f, 1.0f, 1.0f ) );
    texture *volume_dark  = new constant_texture( vec3( 0.0f, 0.0f, 0.0f ) );
    
    list[i++] = new flip_normals(new yz_rect(0,555,0,555,555, green)); // left
    list[i++] = new yz_rect(0,555,0,555,  0, red);                     // right
    list[i++] = new flip_normals(new xz_rect(0,555,0,555,555, white)); // top
    list[i++] = new xz_rect(213,343,227,332,554, light);               // light
    list[i++] = new xz_rect(0,555,0,555,0, white);                     // bottom
    list[i++] = new flip_normals(new xy_rect(0,555,0,555,555, white)); // back
    
    hitable *b1 = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 165, 165), white), -18 ), vec3( 130, 0,  65 ));  // small box
    hitable *b2 = new translate( new rotate_y ( new box(vec3(0,0,0), vec3(165, 330, 165), white),  15 ), vec3( 265, 0, 295 )); // big box
    
    list[i++] = new constant_medium( b1, 0.02f, volume_light );
    list[i++] = new constant_medium( b2, 0.01f, volume_dark );
    
    return new hitable_list(list, i);
}

hitable *mega_big_scene_end_of_book2()
{
    hitable **list = new hitable*[30];
    hitable **boxlist = new hitable*[400];
    hitable **boxlist2 = new hitable*[1000];
    
    material *white  = new lambertian( new constant_texture(vec3(0.73f,0.73f,0.73f)));
    material *ground = new lambertian( new constant_texture(vec3(0.48f,0.83f,0.53f)));
    
    // 20x20 floor of boxes of 100x100xrandom_height
    int nb = 20;
    int b = 0; // total_nb_boxes
    for ( int i = 0; i < nb; ++i )
    {
        for ( int j = 0; j < nb; ++j )
        {
            float w = 100.0f;
            float x0 = -1000.0f + i * w;
            float z0 = -1000.0f + j * w;
            float y0 = 0.0f;
            float x1 = x0 + w;
            float y1 = 100.0f * ( RAN01() + 0.01f );
            float z1 = z0 + w;
            boxlist[b++] = new box( vec3(x0, y0, z0), vec3(x1, y1, z1), ground );
        }
    }
    
    int l = 0;
    list[l++] = new bvh_node( boxlist, b, 0.0f, 1.0f );
    
    // cornell light
    material *light = new diffuse_light( new constant_texture(vec3(7,7,7)));
    list[l++] = new xz_rect(123, 423, 147, 412, 554, light);
    
    // motion blurred sphere
    vec3 center(400,400,200);
    list[l++] = new moving_sphere( 
        center, center + vec3(30,0,0), 0, 1, 50, 
        new lambertian(
        new constant_texture(
        vec3(0.7f, 0.3f, 0.1f)
        )
        )
        );
    
    // glass sphere
    list[l++] = new sphere(vec3(260, 150, 45), 50, new dielectric(1.5f));
    
    // metal sphere
    list[l++] = new sphere(vec3(0, 150, 145), 50, new metal(new constant_texture(vec3(0.8f, 0.8f, 0.9f)), 10.0f));
    
    // subsurface sphere 1
    hitable *boundary = new sphere(vec3(360, 150, 145), 70, new dielectric(1.5f));
    list[l++] = boundary;
    list[l++] = new constant_medium(boundary, 0.2f, new constant_texture(vec3(0.2f, 0.4f, 0.9f)));
    
    // fog over all scene
    boundary = new sphere(vec3(0,0,0),5000, new dielectric(1.5f));
    list[l++] = new constant_medium(boundary, 0.0001f, new constant_texture(vec3(1.0f, 1.0f, 1.0f)));
    
    // textured sphere
    int nx, ny, nn;
    unsigned char *tex_data = stbi_load("../data/earth.jpg", &nx, &ny, &nn, 0);
    material *emat = new lambertian(new image_texture(tex_data, nx, ny));
    list[l++] = new sphere(vec3(400,200,400), 100, emat);
    
    // noise sphere
    texture *perlin_tex = new noise_texture(0.1f);
    list[l++] = new sphere(vec3(220, 280, 300), 80, new lambertian(perlin_tex));
    
    // cube of many spheres
    int ns = 1000;
    for ( int j = 0; j < ns; ++j )
    {
        boxlist2[j] = new sphere(vec3(165.0f*RAN01(),165.0f*RAN01(),165.0f*RAN01()), 10, white );
    }
    list[l++] = new translate( new rotate_y( new bvh_node(boxlist2, ns, 0.0f, 1.0f), 15.0f), vec3(-100.0f,270.0f,395.0f));
    
    
    return new hitable_list(list, l);
}

#endif //_RAYTRACER_SCENES_H
