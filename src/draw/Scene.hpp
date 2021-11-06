#ifndef SHIFVRSKY_SCENE_HPP
#define SHIFVRSKY_SCENE_HPP

struct Scene
{

    virtual void init() = 0;
    virtual void draw(float deltaTime) = 0;
    virtual void deinit() = 0;

};

struct EmptyScene final : public Scene
{
        
    static EmptyScene instance;

    void init() override;
    void draw(float) override;
    void deinit() override;

};

#endif // SHIFVRSKY_SCENE_HPP