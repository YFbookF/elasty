#include <elasty/constraint.hpp>
#include <elasty/particle.hpp>
#include <gtest/gtest.h>

using Eigen::Matrix;
using Eigen::Vector3d;

template <int Num>
void calculateNumericalDerivative(const std::shared_ptr<elasty::Particle>           particles[],
                                  const std::shared_ptr<elasty::AbstractConstraint> constraint,
                                  double*                                           grad)
{
    constexpr double delta = 1e-06;

    for (int i = 0; i < Num; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            Vector3d eps3d = Vector3d::Zero();
            eps3d(j)       = delta;

            const Vector3d orig_pos = particles[i]->p;

            particles[i]->p        = orig_pos + eps3d;
            const double cost_plus = constraint->calculateValue();

            particles[i]->p         = orig_pos - eps3d;
            const double cost_minus = constraint->calculateValue();

            particles[i]->p = orig_pos;

            grad[i * 3 + j] = (cost_plus - cost_minus) / (2.0 * delta);
        }
    }
}

TEST(ConstraintTest, BendingRestShape)
{
    constexpr double dt = 1.0 / 60.0;

    auto p_0 = std::make_shared<elasty::Particle>(Vector3d(0.0, 0.0, 0.0), Vector3d::Zero(), 1.0);
    auto p_1 = std::make_shared<elasty::Particle>(Vector3d(0.0, 1.0, 0.0), Vector3d::Zero(), 1.0);
    auto p_2 = std::make_shared<elasty::Particle>(Vector3d(-0.5, 0.5, 0.0), Vector3d::Zero(), 1.0);
    auto p_3 = std::make_shared<elasty::Particle>(Vector3d(+0.5, 0.5, 0.0), Vector3d::Zero(), 1.0);

    const Vector3d& x_0 = p_0->x;
    const Vector3d& x_1 = p_1->x;
    const Vector3d& x_2 = p_2->x;
    const Vector3d& x_3 = p_3->x;

    const Vector3d p_10 = x_1 - x_0;
    const Vector3d p_20 = x_2 - x_0;
    const Vector3d p_30 = x_3 - x_0;

    const Vector3d n_0 = p_10.cross(p_20).normalized();
    const Vector3d n_1 = p_10.cross(p_30).normalized();

    EXPECT_FALSE(n_0.hasNaN());
    EXPECT_FALSE(n_1.hasNaN());

    const double dihedral_angle = std::acos(std::max(-1.0, std::min(+1.0, n_0.dot(n_1))));

    EXPECT_FALSE(std::isnan(dihedral_angle));

    const auto constraint =
        std::make_shared<elasty::BendingConstraint>(p_0, p_1, p_2, p_3, 1.0, 0.0, dt, dihedral_angle);

    const double value = constraint->calculateValue();

    Matrix<double, 12, 1> grad;
    constraint->calculateGrad(grad.data());

    constexpr double epsilon = 1e-20;

    EXPECT_TRUE(std::abs(value) < epsilon);
    EXPECT_TRUE(grad.norm() < epsilon);
}

TEST(ConstraintTest, BendingDerivative)
{
    constexpr double dt = 1.0 / 60.0;

    auto p_0 = std::make_shared<elasty::Particle>(Vector3d(0.0, 0.0, 0.0), Vector3d::Zero(), 1.0);
    auto p_1 = std::make_shared<elasty::Particle>(Vector3d(0.0, 1.0, 0.0), Vector3d::Zero(), 1.0);
    auto p_2 = std::make_shared<elasty::Particle>(Vector3d(-0.5, 0.5, 0.0), Vector3d::Zero(), 1.0);
    auto p_3 = std::make_shared<elasty::Particle>(Vector3d(+0.5, 0.5, 0.0), Vector3d::Zero(), 1.0);

    const std::shared_ptr<elasty::Particle> particles[] = {p_0, p_1, p_2, p_3};

    const Vector3d& x_0 = p_0->x;
    const Vector3d& x_1 = p_1->x;
    const Vector3d& x_2 = p_2->x;
    const Vector3d& x_3 = p_3->x;

    const Vector3d p_10 = x_1 - x_0;
    const Vector3d p_20 = x_2 - x_0;
    const Vector3d p_30 = x_3 - x_0;

    const Vector3d n_0 = p_10.cross(p_20).normalized();
    const Vector3d n_1 = p_10.cross(p_30).normalized();

    EXPECT_FALSE(n_0.hasNaN());
    EXPECT_FALSE(n_1.hasNaN());

    const double dihedral_angle = std::acos(std::max(-1.0, std::min(+1.0, n_0.dot(n_1))));

    EXPECT_FALSE(std::isnan(dihedral_angle));

    const auto constraint =
        std::make_shared<elasty::BendingConstraint>(p_0, p_1, p_2, p_3, 1.0, 0.0, dt, dihedral_angle);

    constexpr double epsilon = 1e-04;

    for (int i = 0; i < 4; ++i)
    {
        particles[i]->p = particles[i]->x + Eigen::Vector3d::Random();
    }

    Matrix<double, 12, 1> analytic_grad;
    constraint->calculateGrad(analytic_grad.data());

    Matrix<double, 12, 1> numerical_grad;
    calculateNumericalDerivative<4>(particles, constraint, numerical_grad.data());

    EXPECT_TRUE((numerical_grad - analytic_grad).cwiseAbs().maxCoeff() < epsilon);
}

TEST(ConstraintTest, IsometricBendingDerivative)
{
    constexpr double dt = 1.0 / 60.0;

    auto p_0 = std::make_shared<elasty::Particle>(Vector3d(0.0, 0.0, 0.0), Vector3d::Zero(), 1.0);
    auto p_1 = std::make_shared<elasty::Particle>(Vector3d(0.0, 1.0, 0.0), Vector3d::Zero(), 1.0);
    auto p_2 = std::make_shared<elasty::Particle>(Vector3d(-0.5, 0.5, 0.0), Vector3d::Zero(), 1.0);
    auto p_3 = std::make_shared<elasty::Particle>(Vector3d(+0.5, 0.5, 0.0), Vector3d::Zero(), 1.0);

    const std::shared_ptr<elasty::Particle> particles[] = {p_0, p_1, p_2, p_3};

    const auto constraint = std::make_shared<elasty::IsometricBendingConstraint>(p_0, p_1, p_2, p_3, 1.0, 0.0, dt);

    constexpr double epsilon = 1e-04;

    for (int i = 0; i < 4; ++i)
    {
        particles[i]->p = particles[i]->x + Eigen::Vector3d::Random();
    }

    Matrix<double, 12, 1> analytic_grad;
    constraint->calculateGrad(analytic_grad.data());

    Matrix<double, 12, 1> numerical_grad;
    calculateNumericalDerivative<4>(particles, constraint, numerical_grad.data());

    EXPECT_TRUE((numerical_grad - analytic_grad).cwiseAbs().maxCoeff() < epsilon);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
