#include "GameObject.h"

Transform::Transform()
{
	_pos = Vec3f(0.0f, 0.0f, 0.0f);
	_rot = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
	_scale = Vec3f(1.0f, 1.0f, 1.0f);
}


Transform::~Transform()
{
}

void Transform::update()
{
	if (&_oldPos != nullptr)
	{
		_oldPos = _pos;
		_oldRot = _rot;
		_oldScale = _scale;
	}
	else
	{
		_oldPos = _pos + (1.0f);
		_oldRot = _rot *(0.5f);
		_oldScale = _scale + (1.0f);
	}
}

void Transform::rotate(Vec3f axis, float angle)
{
	_rot = Quaternion(axis, angle) * (_rot.normalized());
}

const Vec3f & Transform::getPos()
{
	return _pos;
}

const Quaternion & Transform::getRot()
{
	return _rot;
}

const Vec3f & Transform::getScale()
{
	return _scale;
}

void Transform::setPos(const Vec3f & pos)
{
	_pos = pos;
}

void Transform::setRot(const Quaternion & rot)
{
	_rot = rot;
}

void Transform::setScale(const Vec3f & scale)
{
	_scale = scale;
}

const Vec3f & Transform::getOldPos()
{
	return _oldPos;
}

const Quaternion & Transform::getOldRot()
{
	return _oldRot;
}

const Vec3f & Transform::getOldScale()
{
	return _oldScale;
}

GameComponent::GameComponent()
{
	_name = "defaultRootComponent";
}

GameComponent::~GameComponent()
{
	_parent = nullptr;
	delete _parent;
}

GameObject * GameComponent::getParent()
{
	return _parent;
}

void GameComponent::setParent(GameObject* parent)
{
	_parent = parent;
}

const std::string & GameComponent::getName()
{
	return _name;
}

void GameComponent::setName(const std::string & name)
{
	_name = name;
}

Transform* GameComponent::getTransform()
{
	return _parent->getTransform();
}

void GameComponent::input(float delta)
{
}

void GameComponent::update(float delta)
{
}


GameObject::GameObject()
{
	_parent = nullptr;
	_name = "defaultRootObject";
}


GameObject::~GameObject()
{
	_parent = nullptr;
	delete _parent;
	_children.clear();
	delete &_children;
	_components.clear();
	delete &_components;
}

const std::string & GameObject::getName()
{
	return _name;
}

void GameObject::setName(const std::string & name)
{
	_name = name;
}

void GameObject::addChind(GameObject* child)
{
	_children.push_back(child);
	child->setParent(this);
}

void GameObject::addComponent(GameComponent* component)
{
	_components.push_back(component);
	component->setParent(this);
}

void GameObject::input(float delta)
{
	_transform.update();

	for (auto component : _components)
		component->input(delta);

	for (auto child : _children)
		child->input(delta);
}

void GameObject::update(float delta)
{
	//std::cout << _name +  " is updating." << std::endl;
	for (auto component : _components)
		component->update(delta);

	for (auto child : _children)
		child->update(delta);
}

void GameObject::render(Shader* shader)
{
	//std::cout << _name +  " is rendering." << std::endl;
	for (int i = 0; i < _components.size(); i++)
	{
		_components[i]->render(shader);

	}
	for (int i = 0; i < _children.size(); i++)
	{
		_children[i]->render(shader);
		
	}
}

GameObject * GameObject::getParent()
{
	return _parent;
}

void GameObject::setParent(GameObject * parent)
{
	_parent = parent;
}


Transform* GameObject::getTransform()
{
	return &_transform;
}

RenderingEngine * GameObject::getRenderingEngine()
{
	return _renderingEngine;
}

void GameObject::setRenderingEngine(RenderingEngine* renderingEngine)
{
	_renderingEngine = renderingEngine;
	for (int i = 0; i < _children.size(); i++)
	{
		_children[i]->setRenderingEngine(renderingEngine);
	}
}

bool GameObject::hasTransformChanged()
{
	if (_transform.getPos() != _transform.getOldPos() || _transform.getRot() != _transform.getOldRot() || _transform.getScale() != _transform.getOldScale())
	{
		return true;
	}

	if (_parent != nullptr)
	{
		if (_parent->hasTransformChanged())
		{
			return true;
		}	
	}
	return false;
}

Mat4f GameObject::caclTransformation()
{
	Mat4f translationMatrix;
	translationMatrix.initTranslation(_transform.getPos().getX(), _transform.getPos().getY(), _transform.getPos().getZ());

	Quaternion _tempRot = _transform.getRot();
	Mat4f rotaionMartix = _tempRot.toRotationMatrix();

	Mat4f scaleMartix;
	scaleMartix.initScale(_transform.getScale().getX(), _transform.getScale().getY(), _transform.getScale().getZ());

	Mat4f _parentMatrix;
	_parentMatrix.initIdentity();
	if (_parent != nullptr && _parent->hasTransformChanged())
	{
		_parentMatrix = _parent->caclTransformation();
	}

	return _parentMatrix * translationMatrix * rotaionMartix * scaleMartix;
}

Vec3f GameObject::caclTransformedPos()
{
	Mat4f _parentMatrix;
	_parentMatrix.initIdentity();
	if (_parent != nullptr && _parent->hasTransformChanged())
	{
		_parentMatrix = _parent->caclTransformation();
	}

	return _parentMatrix.transform(_transform.getPos());
}

Quaternion GameObject::caclTransformedRot()
{
	Quaternion _parentRotation = Quaternion(0, 0, 0, 1);

	if (_parent != nullptr)

		_parentRotation = _parent->caclTransformedRot();

	return _parentRotation * _transform.getRot();
}
