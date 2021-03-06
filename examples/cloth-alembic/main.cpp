#include <elasty/cloth-sim-object.hpp>
#include <elasty/constraint.hpp>
#include <elasty/engine.hpp>
#include <elasty/particle.hpp>
#include <elasty/utils.hpp>
#include <timer.hpp>

// #define SPHERE_COLLISION
#define MOVING_SPHERE_COLLISION

#define CLOTH_FALL

class SimpleEngine final : public elasty::AbstractEngine
{
public:
    SimpleEngine() : elasty::AbstractEngine(1.0 / 60.0, 10, 5, elasty::AlgorithmType::Xpbd) {}

    void initializeScene() override
    {
        // Instantiate a cloth object
        constexpr double   cloth_in_plane_stiffness      = 1.000; ///< PBD
        constexpr double   cloth_in_plane_compliance     = 5e-02; ///< XPBD
        constexpr double   cloth_out_of_plane_stiffness  = 0.100; ///< PBD
        constexpr double   cloth_out_of_plane_compliance = 5e+04; ///< XPBD
        constexpr unsigned cloth_resolution              = 50;

        const Eigen::Affine3d cloth_import_transform =
#if defined(CLOTH_FALL)
            Eigen::Affine3d(Eigen::Translation3d(0.0, 2.0, 1.0));
#else
            Eigen::Translation3d(0.0, 1.0, 0.0) * Eigen::AngleAxisd(0.5 * elasty::pi(), Eigen::Vector3d::UnitX());
#endif

        m_cloth_sim_object =
            std::make_shared<elasty::ClothSimObject>(cloth_resolution,
                                                     cloth_in_plane_stiffness,
                                                     cloth_in_plane_compliance,
                                                     cloth_out_of_plane_stiffness,
                                                     cloth_out_of_plane_compliance,
                                                     getDeltaPhysicsTime(),
                                                     cloth_import_transform,
                                                     elasty::ClothSimObject::InPlaneStrategy::EdgeDistance,
                                                     elasty::ClothSimObject::OutOfPlaneStrategy::IsometricBending);

        // Register the cloth object
        std::copy(m_cloth_sim_object->m_particles.begin(),
                  m_cloth_sim_object->m_particles.end(),
                  std::back_inserter(m_particles));
        std::copy(m_cloth_sim_object->m_constraints.begin(),
                  m_cloth_sim_object->m_constraints.end(),
                  std::back_inserter(m_constraints));

        // Add small perturb
        for (const auto& particle : m_particles)
        {
            particle->v = 1e-03 * Eigen::Vector3d::Random();
        }

        // Pin two of the corners of the cloth
        constexpr double range_radius = 0.1;
        for (const auto& particle : m_particles)
        {
            if ((particle->x - Eigen::Vector3d(+1.0, 2.0, 0.0)).norm() < range_radius)
            {
                m_constraints.push_back(std::make_shared<elasty::FixedPointConstraint>(
                    particle, 1.0, 0.0, getDeltaPhysicsTime(), particle->x));
            }
            if ((particle->x - Eigen::Vector3d(-1.0, 2.0, 0.0)).norm() < range_radius)
            {
                m_constraints.push_back(std::make_shared<elasty::FixedPointConstraint>(
                    particle, 1.0, 0.0, getDeltaPhysicsTime(), particle->x));
            }
        }
    }

    void setExternalForces() override
    {
        const auto gravity = Eigen::Vector3d{0.0, -9.8, 0.0};

        for (auto particle : m_particles)
        {
            particle->f = particle->m * gravity;
        }

        // Aerodynamic force for cloth
        m_cloth_sim_object->applyAerodynamicForces();
    }

    void generateCollisionConstraints() override
    {
#if defined(SPHERE_COLLISION)
        // Collision with a sphere
        const Eigen::Vector3d center(0.0, 1.0, 0.0);
        constexpr double      tolerance  = 0.05;
        constexpr double      radius     = 0.50 + 0.02;
        constexpr double      stiffness  = 1.00;
        constexpr double      compliance = 0.00;
        for (auto particle : m_particles)
        {
            const Eigen::Vector3d direction = particle->x - center;
            if (direction.norm() < radius + tolerance)
            {
                const Eigen::Vector3d normal   = direction.normalized();
                const double          distance = center.transpose() * normal + radius;
                m_instant_constraints.push_back(std::make_shared<elasty::EnvironmentalCollisionConstraint>(
                    particle, stiffness, compliance, m_dt, normal, distance));
            }
        }
#elif defined(MOVING_SPHERE_COLLISION)
        // Collision with a moving sphere
        const Eigen::Vector3d center(0.0, 1.0, std::max(0.0, (getCurrentPhysicsTime() - 1.80)));
        constexpr double      tolerance  = 0.05;
        constexpr double      radius     = 0.50 + 0.02;
        constexpr double      stiffness  = 1.00;
        constexpr double      compliance = 0.00;
        for (auto particle : m_particles)
        {
            const Eigen::Vector3d direction = particle->x - center;
            if (direction.norm() < radius + tolerance)
            {
                const Eigen::Vector3d normal   = direction.normalized();
                const double          distance = center.transpose() * normal + radius;
                m_instant_constraints.push_back(std::make_shared<elasty::EnvironmentalCollisionConstraint>(
                    particle, stiffness, compliance, getDeltaPhysicsTime(), normal, distance));
            }
        }
#endif
    }

    void updateVelocities() override
    {
        const double decay_rate = std::exp(std::log(0.95) * getDeltaPhysicsTime());

        for (auto particle : m_particles)
        {
            particle->v *= decay_rate;
        }
    }

    std::shared_ptr<elasty::ClothSimObject> m_cloth_sim_object;
};

int main(int argc, char** argv)
{
    SimpleEngine engine;
    engine.initializeScene();

    auto alembic_manager =
        elasty::createAlembicManager("./cloth.abc", engine.m_cloth_sim_object, engine.getDeltaFrameTime());

    for (unsigned int frame = 0; frame < 300; ++frame)
    {
        timer::Timer t(std::to_string(frame));
        elasty::submitCurrentStatus(alembic_manager);

        engine.proceedFrame();
    }

    return 0;
}
