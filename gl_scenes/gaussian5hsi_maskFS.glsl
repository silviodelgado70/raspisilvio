#extension GL_OES_EGL_image_external : require

uniform samplerExternalOES tex;
varying vec2 texcoord;
uniform vec2 tex_unit;
uniform sampler2D mask;

void main(void)
{
    vec4 mask_col = texture2D(mask, texcoord);

    if (mask_col.g == 0.0)
    {
        gl_FragColor = vec4(0);
        return;
    }

    vec4 col = vec4(0);

    float x = texcoord.x;
    float y = texcoord.y;
    float xm1 = x - tex_unit.x;
    float ym1 = y - tex_unit.y;
    float xm2 = x - 2.0*tex_unit.x;
    float ym2 = y - 2.0*tex_unit.y;
    float xp1 = x + tex_unit.x;
    float yp1 = y + tex_unit.y;
    float xp2 = x + 2.0*tex_unit.x;
    float yp2 = y + 2.0*tex_unit.y;

    col += 1.0/256.0 * texture2D(tex, vec2(xm2, ym2));
    col += 4.0/256.0 * texture2D(tex, vec2(xm1, ym2));
    col += 6.0/256.0 * texture2D(tex, vec2(x, ym2));
    col += 4.0/256.0 * texture2D(tex, vec2(xp1, ym2));
    col += 1.0/256.0 * texture2D(tex, vec2(xp2, ym2));

    col += 4.0/256.0 * texture2D(tex, vec2(xm2, ym1));
    col += 16.0/256.0 * texture2D(tex, vec2(xm1, ym1));
    col += 24.0/256.0 * texture2D(tex, vec2(x, ym1));
    col += 16.0/256.0 * texture2D(tex, vec2(xp1, ym1));
    col += 4.0/256.0 * texture2D(tex, vec2(xp2, ym1));

    col += 6.0/256.0 * texture2D(tex, vec2(xm2, y));
    col += 24.0/256.0 * texture2D(tex, vec2(xm1, y));
    col += 36.0/256.0 * texture2D(tex, vec2(x, y));
    col += 24.0/256.0 * texture2D(tex, vec2(xp1, y));
    col += 6.0/256.0 * texture2D(tex, vec2(xp2, y));

    col += 4.0/256.0 * texture2D(tex, vec2(xm2, yp1));
    col += 16.0/256.0 * texture2D(tex, vec2(xm1, yp1));
    col += 24.0/256.0 * texture2D(tex, vec2(x, yp1));
    col += 16.0/256.0 * texture2D(tex, vec2(xp1, yp1));
    col += 4.0/256.0 * texture2D(tex, vec2(xp2, yp1));

    col += 1.0/256.0 * texture2D(tex, vec2(xm2, yp2));
    col += 4.0/256.0 * texture2D(tex, vec2(xm1, yp2));
    col += 6.0/256.0 * texture2D(tex, vec2(x, yp2));
    col += 4.0/256.0 * texture2D(tex, vec2(xp1, yp2));
    col += 1.0/256.0 * texture2D(tex, vec2(xp2, yp2));

    float temp = max(col.r,col.g);
    float max_col = max(col.b,temp);

    float temp2 = min(col.r,col.g);
    float min_col = min(col.b,temp2);

    float i = (col.r + col.g + col.b) / 3.0;

    float c = max_col - min_col;

    float s = 0.0;

    if(c != 0.0)
    {
        s = 1.0 - min_col / i;
    }

    float h = 0.0;

    if(max_col == col.r)
    {
        h = (60.0 * abs(col.g - col.b)/c) / 360.0 ;
    }
    else if(max_col  == col.g)
    {
        h = (120.0 + 60.0 * abs(col.b - col.r)/c) / 360.0 ;
    }
    else if(max_col == col.b)
    {
        h = (240.0 + 60.0 * abs(col.r - col.g)/c) / 360.0 ;
    }

    gl_FragColor = vec4(h,s,i,1.0);
}