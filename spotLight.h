//
//  spotLight.h
//  Premium Living Room Project
//
//  Modeled after pointLight.h - adds direction + cone angles
//

#ifndef spotLight_h
#define spotLight_h

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "shader.h"

class SpotLight {
public:
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float k_c;
    float k_l;
    float k_q;
    float cutOff;       // cosine of inner cone angle
    float outerCutOff;  // cosine of outer cone angle

    SpotLight(glm::vec3 pos, glm::vec3 dir,
              glm::vec3 amb, glm::vec3 diff, glm::vec3 spec,
              float constant, float linear, float quadratic,
              float innerAngleDeg, float outerAngleDeg)
    {
        position = pos;
        direction = glm::normalize(dir);
        ambient = amb;
        diffuse = diff;
        specular = spec;
        k_c = constant;
        k_l = linear;
        k_q = quadratic;
        cutOff = cos(glm::radians(innerAngleDeg));
        outerCutOff = cos(glm::radians(outerAngleDeg));
    }

    void setUpSpotLight(Shader& shader)
    {
        shader.use();
        shader.setVec3("spotLight.position", position);
        shader.setVec3("spotLight.direction", direction);
        shader.setVec3("spotLight.ambient", ambientOn * ambient);
        shader.setVec3("spotLight.diffuse", diffuseOn * diffuse);
        shader.setVec3("spotLight.specular", specularOn * specular);
        shader.setFloat("spotLight.k_c", k_c);
        shader.setFloat("spotLight.k_l", k_l);
        shader.setFloat("spotLight.k_q", k_q);
        shader.setFloat("spotLight.cutOff", cutOff);
        shader.setFloat("spotLight.outerCutOff", outerCutOff);
    }

    void turnOn()  { ambientOn = 1.0f; diffuseOn = 1.0f; specularOn = 1.0f; }
    void turnOff() { ambientOn = 0.0f; diffuseOn = 0.0f; specularOn = 0.0f; }

private:
    float ambientOn = 1.0f;
    float diffuseOn = 1.0f;
    float specularOn = 1.0f;
};

#endif /* spotLight_h */
