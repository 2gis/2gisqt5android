#define FP highp

varying FP vec3 lightDir;
varying FP vec3 viewDir;
varying FP vec2 texCoord;

uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D normalTexture;

uniform FP vec4 lightPosition;
uniform FP vec3 lightIntensity;

// TODO: Replace with a struct
uniform FP vec3 ka;            // Ambient reflectivity
uniform FP float shininess;    // Specular shininess factor

FP vec3 adsModel( const FP vec3 norm, const FP vec3 diffuseReflect, const FP vec3 specular )
{
    // Reflection of light direction about normal
    FP vec3 r = reflect( -lightDir, norm );

    // Calculate the ambient contribution
    FP vec3 ambient = lightIntensity * ka;

    // Calculate the diffuse contribution
    FP float sDotN = max( dot( lightDir, norm ), 0.0 );
    FP vec3 diffuse = lightIntensity * diffuseReflect * sDotN;

    // Sum the ambient and diffuse contributions
    FP vec3 ambientAndDiff = ambient + diffuse;

    // Calculate the specular highlight contribution
    FP vec3 spec = vec3( 0.0 );
    if ( sDotN > 0.0 )
        spec = lightIntensity * ( shininess / ( 8.0 * 3.14 ) ) * pow( max( dot( r, viewDir ), 0.0 ), shininess );
    return (ambientAndDiff + spec * specular.rgb);
}

void main()
{
    // Sample the textures at the interpolated texCoords
    FP vec4 diffuseTextureColor = texture2D( diffuseTexture, texCoord );
    FP vec4 specularTextureColor = texture2D( specularTexture, texCoord );
    FP vec4 normal = 2.0 * texture2D( normalTexture, texCoord ) - vec4( 1.0 );

    // Calculate the lighting model
    gl_FragColor = vec4( adsModel( normalize( normal.xyz ), diffuseTextureColor.xyz, specularTextureColor.xyz ), 1.0 );
}
