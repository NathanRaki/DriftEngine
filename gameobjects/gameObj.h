#ifndef GAME_OBJ_H
#define GAME_OBJ_H

#include "includes/glm/glm.hpp"
#include "includes/glm/gtc/matrix_transform.hpp"

class GameObject
{
public:
	glm::vec3 worldPosition;
	glm::vec3 scale;

	GameObject();
	GameObject(	glm::vec3 wPos, glm::vec3 scale);
};

#endif