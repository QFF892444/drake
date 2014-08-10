#include "drakeGeometryUtil.h"
#include <iostream>
#include <cmath>
#include <limits>

using namespace Eigen;

double angleDiff(double phi1, double phi2)
{
  double d = phi2-phi1;
  if(d>0.0)
  {
    d = fmod(d+M_PI,2*M_PI)-M_PI;
  }
  else
  {
    d = fmod(d-M_PI,2*M_PI)+M_PI;
  }
  return d;
}

Vector4d quatConjugate(const Eigen::Vector4d& q)
{
  Vector4d q_conj;
  q_conj << q(0), -q(1), -q(2), -q(3);
  return q_conj;
}

Eigen::Matrix4d dquatConjugate()
{
  Matrix4d dq_conj = Matrix4d::Identity();
  dq_conj(1, 1) = -1.0;
  dq_conj(2, 2) = -1.0;
  dq_conj(3, 3) = -1.0;
  return dq_conj;
}

Eigen::Vector4d quatProduct(const Eigen::Vector4d& q1, const Eigen::Vector4d& q2)
{
  double w1 = q1(0);
  double w2 = q2(0);
  const auto& v1 = q1.tail<3>();
  const auto& v2 = q2.tail<3>();
  Vector4d r;
  r << w1 * w2 - v1.dot(v2), v1.cross(v2) + w1 * v2 + w2 * v1;
  return r;
}

Eigen::Matrix<double, 4, 8> dquatProduct(const Eigen::Vector4d& q1, const Eigen::Vector4d& q2)
{
  double w1 = q1(0);
  double w2 = q2(0);
  const auto& v1 = q1.tail<3>();
  const auto& v2 = q2.tail<3>();

  Matrix<double, 4, 8> dr;
  dr.row(0) << w2, -v2.transpose(), w1, -v1.transpose();
  dr.row(1) << q2(1), q2(0), q2(3), -q2(2), q1(1), q1(0), -q1(3), q1(2);
  dr.row(2) << q2(2), -q2(3), q2(0), q2(1), q1(2), q1(3), q1(0), -q1(1);
  dr.row(3) << q2(3), q2(2), -q2(1), q2(0), q1(3), -q1(2), q1(1), q1(0);
  return dr;
}

Eigen::Vector3d quatRotateVec(const Eigen::Vector4d& q, const Eigen::Vector3d& v)
{
  Vector4d v_quat;
  v_quat << 0, v;
  Vector4d q_times_v = quatProduct(q, v_quat);
  Vector4d q_conj = quatConjugate(q);
  Vector4d v_rot = quatProduct(q_times_v, q_conj);
  Vector3d r = v_rot.bottomRows<3>();
  return r;
}

Eigen::Matrix<double, 3, 7> dquatRotateVec(const Eigen::Vector4d& q, const Eigen::Vector3d& v)
{
  Matrix<double, 4, 7> dq;
  dq << Matrix4d::Identity(), MatrixXd::Zero(4, 3);
  Matrix<double, 4, 7> dv = Matrix<double, 4, 7>::Zero();
  dv.bottomRightCorner<3, 3>() = Matrix3d::Identity();
  Matrix<double, 8, 7> dqdv;
  dqdv << dq, dv;

  Vector4d v_quat;
  v_quat << 0, v;
  Vector4d q_times_v = quatProduct(q, v_quat);
  Matrix<double, 4, 8> dq_times_v_tmp = dquatProduct(q, v_quat);
  Matrix<double, 4, 7> dq_times_v = dq_times_v_tmp * dqdv;

  Matrix<double, 4, 7> dq_conj = dquatConjugate() * dq;
  Matrix<double, 8, 7> dq_times_v_dq_conj;
  dq_times_v_dq_conj << dq_times_v, dq_conj;
  Matrix<double, 4, 8> dv_rot_tmp = dquatProduct(q_times_v, quatConjugate(q));
  Matrix<double, 4, 7> dv_rot = dv_rot_tmp * dq_times_v_dq_conj;
  Eigen::Matrix<double, 3, 7> dr = dv_rot.bottomRows(3);
  return dr;
}

Eigen::Vector4d quatDiff(const Eigen::Vector4d& q1, const Eigen::Vector4d& q2)
{
  return quatProduct(quatConjugate(q1), q2);
}

Eigen::Matrix<double, 4, 8> dquatDiff(const Eigen::Vector4d& q1, const Eigen::Vector4d& q2)
{
  auto dr = dquatProduct(quatConjugate(q1), q2);
  dr.block<4, 3>(0, 1) = -dr.block<4, 3>(0, 1);
  return dr;
}

double quatDiffAxisInvar(const Eigen::Vector4d& q1, const Eigen::Vector4d& q2, const Eigen::Vector3d& u)
{
  Vector4d r = quatDiff(q1, q2);
  double e = -2.0 + 2 * r(0) * r(0) + 2 * pow(u(0) * r(1) + u(1) * r(2) + u(2) * r(3), 2);
  return e;
}

Eigen::Matrix<double, 1, 11> dquatDiffAxisInvar(const Eigen::Vector4d& q1, const Eigen::Vector4d& q2, const Eigen::Vector3d& u)
{
  Vector4d r = quatDiff(q1, q2);
  Matrix<double, 4, 8> dr = dquatDiff(q1, q2);
  Matrix<double, 1, 11> de;
  const auto& rvec = r.tail<3>();
  de << 4.0 * r(0) * dr.row(0) + 4.0 * u.transpose() * rvec *u.transpose() * dr.block<3, 8>(1, 0), 4.0 * u.transpose() * rvec * rvec.transpose();
  return de;
}

double quatNorm(const Eigen::Vector4d& q)
{
  return std::acos(q(0));
}

Vector4d uniformlyRandomAxisAngle(std::default_random_engine& generator)
{
  std::normal_distribution<double> normal;
  std::uniform_real_distribution<double> uniform(-M_PI, M_PI);
  double angle = uniform(generator);
  Vector3d axis = Vector3d(normal(generator), normal(generator), normal(generator));
  axis.normalize();
  Vector4d a;
  a << axis, angle;
  return a;
}

Vector4d uniformlyRandomQuat(std::default_random_engine& generator)
{
  return axis2quat(uniformlyRandomAxisAngle(generator));
}

Eigen::Matrix3d uniformlyRandomRotmat(std::default_random_engine& generator)
{
  return axis2rotmat(uniformlyRandomAxisAngle(generator));
}

Eigen::Vector3d uniformlyRandomRPY(std::default_random_engine& generator)
{
  return axis2rpy(uniformlyRandomAxisAngle(generator));
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 3, 1> quat2rpy(const Eigen::MatrixBase<Derived>& q)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 4);
  auto q_normalized = q.normalized();
  auto w = q_normalized(0);
  auto x = q_normalized(1);
  auto y = q_normalized(2);
  auto z = q_normalized(3);

  Eigen::Matrix<typename Derived::Scalar, 3, 1> ret;
  ret << std::atan2(2.0*(w*x + y*z), w*w + z*z -(x*x +y*y)),
      std::asin(2.0*(w*y - z*x)),
      std::atan2(2.0*(w*z + x*y), w*w + x*x-(y*y+z*z));
  return ret;
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 3, 3> quat2rotmat(const Eigen::MatrixBase<Derived>& q)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 4);
  auto q_normalized = q.normalized();
  auto w = q_normalized(0);
  auto x = q_normalized(1);
  auto y = q_normalized(2);
  auto z = q_normalized(3);

  Eigen::Matrix<typename Derived::Scalar, 3, 3> M;
  M.row(0) << w * w + x * x - y * y - z * z, 2.0 * x * y - 2.0 * w * z, 2.0 * x * z + 2.0 * w * y;
  M.row(1) << 2.0 * x * y + 2.0 * w * z, w * w + y * y - x * x - z * z, 2.0 * y * z - 2.0 * w * x;
  M.row(2) << 2.0 * x * z - 2.0 * w * y, 2.0 * y * z + 2.0 * w * x, w * w + z * z - x * x - y * y;

  return M;
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 4, 1> quat2axis(const Eigen::MatrixBase<Derived>& q)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 4);
  auto q_normalized = q.normalized();
  auto s = std::sqrt(1.0 - q_normalized(0) * q_normalized(0)) + std::numeric_limits<typename Derived::Scalar>::epsilon();
  Eigen::Matrix<typename Derived::Scalar, 4, 1> a;

  a << q_normalized.template tail<3>() / s, 2.0 * std::acos(q_normalized(0));
  return a;
}

template <typename Derived>
Eigen::Vector4d axis2quat(const Eigen::MatrixBase<Derived>& a)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 4);
  auto axis = a.template head<3>();
  auto angle = a(3);
  auto arg = 0.5 * angle;
  auto c = std::cos(arg);
  auto s = std::sin(arg);
  Eigen::Vector4d ret;
  ret << c, s * axis;
  return ret;
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 3, 3> axis2rotmat(const Eigen::MatrixBase<Derived>& a)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 4);
  const auto& axis = a.template head<3>();
  const auto& theta = a(3);
  auto x = axis(0);
  auto y = axis(1);
  auto z = axis(2);
  auto ctheta = std::cos(theta);
  auto stheta = std::sin(theta);
  auto c = 1 - ctheta;
  Eigen::Matrix<typename Derived::Scalar, 3, 3> R;
  R <<
      ctheta + x * x * c , x * y * c - z * stheta, x * z * c + y * stheta,
      y * x * c + z * stheta, ctheta + y * y * c, y * z * c - x * stheta,
      z * x * c - y * stheta, z * y * c + x * stheta, ctheta + z * z * c;

  return R;
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 3, 1> axis2rpy(const Eigen::MatrixBase<Derived>& a)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 4);
  return quat2rpy(axis2quat(a));
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 4, 1> rotmat2axis(const Eigen::MatrixBase<Derived>& R)
{
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 3, 3);

  typename Derived::Scalar theta = std::acos((R.trace() - 1.0) / 2.0);
  Vector4d a;
  if (theta > std::numeric_limits<typename Derived::Scalar>::epsilon()) {
    a << R(2, 1) - R(1, 2), R(0, 2) - R(2, 0), R(1, 0) - R(0, 1), theta;
    a.head<3>() *= 1.0 / (2.0 * std::sin(theta));
  }
  else {
    a << 1.0, 0.0, 0.0, 0.0;
  }
  return a;
}

template <typename Derived>
Eigen::Matrix<typename Derived::Scalar, 4, 1> rotmat2quat(const Eigen::MatrixBase<Derived>& M)
{
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 3, 3);
  using namespace std;

  Matrix<typename Derived::Scalar, 4, 3> A;
  A.row(0) << 1.0, 1.0, 1.0;
  A.row(1.0) << 1.0, -1.0, -1.0;
  A.row(2) << -1.0, 1.0, -1.0;
  A.row(3) << -1.0, -1.0, 1.0;
  Matrix<typename Derived::Scalar, 4, 1> B = A * M.diagonal();
  typename Matrix<typename Derived::Scalar, 4, 1>::Index ind, max_col;
  typename Derived::Scalar val = B.maxCoeff(&ind, &max_col);

  typename Derived::Scalar w, x, y, z;
  switch (ind) {
  case 0: {
    // val = trace(M)
    w = sqrt(1.0 + val) / 2.0;
    typename Derived::Scalar w4 = w * 4.0;
    x = (M(2, 1) - M(1, 2)) / w4;
    y = (M(0, 2) - M(2, 0)) / w4;
    z = (M(1, 0) - M(0, 1)) / w4;
    break;
  }
  case 1: {
    // val = M(1,1) - M(2,2) - M(3,3)
    double s = 2.0 * sqrt(1.0 + val);
    w = (M(2, 1) - M(1, 2)) / s;
    x = 0.25 * s;
    y = (M(0, 1) + M(1, 0)) / s;
    z = (M(0, 2) + M(2, 0)) / s;
    break;
  }
  case 2: {
    //  % val = M(2,2) - M(1,1) - M(3,3)
    double s = 2.0 * (sqrt(1.0 + val));
    w = (M(0, 2) - M(2, 0)) / s;
    x = (M(0, 1) + M(1, 0)) / s;
    y = 0.25 * s;
    z = (M(1, 2) + M(2, 1)) / s;
    break;
  }
  default: {
    // val = M(3,3) - M(2,2) - M(1,1)
    double s = 2.0 * (sqrt(1.0 + val));
    w = (M(1, 0) - M(0, 1)) / s;
    x = (M(0, 2) + M(2, 0)) / s;
    y = (M(1, 2) + M(2, 1)) / s;
    z = 0.25 * s;
    break;
  }
  }

  Eigen::Matrix<typename Derived::Scalar, 4, 1> q;
  q << w, x, y, z;
  return q;
}

template<typename Derived>
Eigen::Matrix<typename Derived::Scalar, 3, 1> rotmat2rpy(const Eigen::MatrixBase<Derived>& R)
{
  EIGEN_STATIC_ASSERT_MATRIX_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 3, 3);
  using namespace std;

  Eigen::Matrix<typename Derived::Scalar, 3, 1> rpy;
  rpy << atan2(R(2, 1), R(2, 2)), atan2(-R(2, 0), sqrt(pow(R(2, 1), 2.0) + pow(R(2, 2), 2.0))), atan2(R(1, 0), R(0, 0));
  return rpy;
}

template<typename Derived>
Eigen::Matrix<typename Derived::Scalar, 4, 1> rpy2axis(const Eigen::MatrixBase<Derived>& rpy)
{
  return quat2axis(rpy2quat(rpy));
}

template<typename Derived>
Eigen::Matrix<typename Derived::Scalar, 4, 1> rpy2quat(const Eigen::MatrixBase<Derived>& rpy)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 3);
  auto rpy_2 = (rpy / 2.0).array();
  auto s = rpy_2.sin();
  auto c = rpy_2.cos();

  Vector4d q;
  q << c(0)*c(1)*c(2) + s(0)*s(1)*s(2),
        s(0)*c(1)*c(2) - c(0)*s(1)*s(2),
        c(0)*s(1)*c(2) + s(0)*c(1)*s(2),
        c(0)*c(1)*s(2) - s(0)*s(1)*c(2);

  q /= q.norm() + std::numeric_limits<typename Derived::Scalar>::epsilon();
  return q;
}

template<typename Derived>
Eigen::Matrix<typename Derived::Scalar, 3, 3> rpy2rotmat(const Eigen::MatrixBase<Derived>& rpy)
{
  EIGEN_STATIC_ASSERT_VECTOR_SPECIFIC_SIZE(Eigen::MatrixBase<Derived>, 3);
  auto rpy_array = rpy.array();
  auto s = rpy_array.sin();
  auto c = rpy_array.cos();

  Eigen::Matrix<typename Derived::Scalar, 3, 3> R;
  R.row(0) << c(2) * c(1), c(2) * s(1) * s(0) - s(2) * c(0), c(2) * s(1) * c(0) + s(2) * s(0);
  R.row(1) << s(2) * c(1), s(2) * s(1) * s(0) + c(2) * c(0), s(2) * s(1) * c(0) - c(2) * s(0);
  R.row(2) << -s(1), c(1) * s(0), c(1) * c(0);

  return R;
}

template<typename Scalar, typename DerivedS, typename DerivedQdotToV>
Eigen::Matrix<Scalar, HomogeneousTransformSize, DerivedQdotToV::ColsAtCompileTime> dHomogTrans(
    const Eigen::Transform<Scalar, 3, Eigen::Isometry>& T,
    const Eigen::MatrixBase<DerivedS>& S,
    const Eigen::MatrixBase<DerivedQdotToV>& qdot_to_v) {
  const int nq_at_compile_time = DerivedQdotToV::ColsAtCompileTime;
  int nq = qdot_to_v.cols();
  auto qdot_to_twist = S * qdot_to_v;

  const int numel = HomogeneousTransformSize;
  Eigen::Matrix<Scalar, numel, nq_at_compile_time> ret(numel, nq);

  const auto& Rx = T.linear().col(0);
  const auto& Ry = T.linear().col(1);
  const auto& Rz = T.linear().col(2);

  const auto& qdot_to_omega_x = qdot_to_twist.row(0);
  const auto& qdot_to_omega_y = qdot_to_twist.row(1);
  const auto& qdot_to_omega_z = qdot_to_twist.row(2);

  ret.template middleRows<3>(0) = -Rz * qdot_to_omega_y + Ry * qdot_to_omega_z;
  ret.row(3).setZero();

  ret.template middleRows<3>(4) = Rz * qdot_to_omega_x - Rx * qdot_to_omega_z;
  ret.row(7).setZero();

  ret.template middleRows<3>(8) = -Ry * qdot_to_omega_x + Rx * qdot_to_omega_y;
  ret.row(11).setZero();

  ret.template middleRows<3>(12) = T.linear() * qdot_to_twist.bottomRows(3);
  ret.row(15).setZero();

  return ret;
}

template<typename Scalar, typename DerivedDT>
Eigen::Matrix<Scalar, HomogeneousTransformSize, DerivedDT::ColsAtCompileTime> dHomogTransInv(
    const Eigen::Transform<Scalar, 3, Eigen::Isometry>& T,
    const Eigen::MatrixBase<DerivedDT>& dT) {
  const int nq_at_compile_time = DerivedDT::ColsAtCompileTime;
  const int nq = dT.cols();

  const auto& R = T.linear();
  const auto& p = T.translation();

  std::array<int, 3> rows {0, 1, 2};
  std::array<int, 3> R_cols {0, 1, 2};
  std::array<int, 1> p_cols {3};

  const auto dR = getSubMatrixGradient(dT, rows, R_cols, T.Rows);
  const auto dp = getSubMatrixGradient(dT, rows, p_cols, T.Rows);

  auto dinvT_R = transposeGrad(dR, R.rows());
  auto dinvT_p = -R.transpose() * dp - matGradMult(dinvT_R, p);

  const int numel = HomogeneousTransformSize;
  Eigen::Matrix<Scalar, numel, nq_at_compile_time> ret(numel, nq);
  setSubMatrixGradient(ret, dinvT_R, rows, R_cols, T.Rows);
  setSubMatrixGradient(ret, dinvT_p, rows, p_cols, T.Rows);

  // zero out gradient of elements in last row:
  const int last_row = 3;
  for (int col = 0; col < T.HDim; col++) {
    ret.row(last_row + col * T.Rows).setZero();
  }

  return ret;
}

template <typename Scalar, typename DerivedX, typename DerivedDT, typename DerivedDX>
typename Gradient<DerivedX, DerivedDX::ColsAtCompileTime, 1>::type dTransformAdjoint(
    const Eigen::Transform<Scalar, 3, Eigen::Isometry>& T,
    const Eigen::MatrixBase<DerivedX>& X,
    const Eigen::MatrixBase<DerivedDT>& dT,
    const Eigen::MatrixBase<DerivedDX>& dX) {
  assert(dT.cols() == dX.cols());
  const int nq = dT.cols();

  const auto& R = T.linear();
  const auto& p = T.translation();

  const std::array<int, 3> rows {0, 1, 2};
  const std::array<int, 3> R_cols {0, 1, 2};
  const std::array<int, 1> p_cols {3};

  const auto dR = getSubMatrixGradient(dT, rows, R_cols, T.Rows);
  const auto dp = getSubMatrixGradient(dT, rows, p_cols, T.Rows);

  typename Gradient<DerivedX, DerivedDX::ColsAtCompileTime, 1>::type ret(X.size(), nq);
  std::array<int, 3> Xomega_rows {0, 1, 2};
  std::array<int, 3> Xv_rows {3, 4, 5};
  for (int col = 0; col < X.cols(); col++) {
    const auto Xomega_col = X.template block<3, 1>(0, col);
    const auto Xv_col = X.template block<3, 1>(3, col);

    const auto RXomega_col = R * Xomega_col;

    std::array<int, 1> col_array {col};
    const auto dXomega_col = getSubMatrixGradient(dX, Xomega_rows, col_array, X.rows());
    const auto dXv_col = getSubMatrixGradient(dX, Xv_rows, col_array, X.rows());

    const auto dRXomega_col = R * dXomega_col + matGradMult(dR, Xomega_col);
    const auto dRXv_col = R * dXv_col + matGradMult(dR, Xv_col);

    const auto dp_hatRXomega_col = (dp.colwise().cross(RXomega_col) - dRXomega_col.colwise().cross(p)).eval();

    setSubMatrixGradient(ret, dRXomega_col, Xomega_rows, col_array, X.rows());
    setSubMatrixGradient(ret, dp_hatRXomega_col + dRXv_col, Xv_rows, col_array, X.rows());
  }
  return ret;
}

template <typename Scalar, typename DerivedX, typename DerivedDT, typename DerivedDX>
typename Gradient<DerivedX, DerivedDX::ColsAtCompileTime>::type dTransformAdjointTranspose(
    const Eigen::Transform<Scalar, 3, Eigen::Isometry>& T,
    const Eigen::MatrixBase<DerivedX>& X,
    const Eigen::MatrixBase<DerivedDT>& dT,
    const Eigen::MatrixBase<DerivedDX>& dX) {
  assert(dT.cols() == dX.cols());
  const int nq = dT.cols();

  const auto& R = T.linear();
  const auto& p = T.translation();

  const std::array<int, 3> rows {0, 1, 2};
  const std::array<int, 3> R_cols {0, 1, 2};
  const std::array<int, 1> p_cols {3};

  const auto dR = getSubMatrixGradient(dT, rows, R_cols, T.Rows);
  const auto dp = getSubMatrixGradient(dT, rows, p_cols, T.Rows);

  const auto Rtranspose = R.transpose();
  const auto dRtranspose = transposeGrad(dR, R.rows());

  typename Gradient<DerivedX, DerivedDX::ColsAtCompileTime>::type ret(X.size(), nq);
  std::array<int, 3> Xomega_rows {0, 1, 2};
  std::array<int, 3> Xv_rows {3, 4, 5};
  for (int col = 0; col < X.cols(); col++) {
    const auto Xomega_col = X.template block<3, 1>(0, col);
    const auto Xv_col = X.template block<3, 1>(3, col);

    std::array<int, 1> col_array {col};
    const auto dXomega_col = getSubMatrixGradient(dX, Xomega_rows, col_array, X.rows());
    const auto dXv_col = getSubMatrixGradient(dX, Xv_rows, col_array, X.rows());

    const auto dp_hatXv_col = (dp.colwise().cross(Xv_col) - dXv_col.colwise().cross(p)).eval();
    const auto dXomega_transformed_col = Rtranspose * (dXomega_col - dp_hatXv_col) + matGradMult(dRtranspose, Xomega_col - p.cross(Xv_col));
    const auto dRtransposeXv_col = Rtranspose * dXv_col + matGradMult(dRtranspose, Xv_col);

    setSubMatrixGradient(ret, dXomega_transformed_col, Xomega_rows, col_array, X.rows());
    setSubMatrixGradient(ret, dRtransposeXv_col, Xv_rows, col_array, X.rows());
  }
  return ret;
}

// NOTE: not reshaping second derivative to Matlab geval output format!
template <typename Derived>
void normalizeVec(
    const Eigen::MatrixBase<Derived>& x,
    typename Derived::PlainObject& x_norm,
    typename Gradient<Derived, Derived::RowsAtCompileTime, 1>::type* dx_norm,
    typename Gradient<Derived, Derived::RowsAtCompileTime, 2>::type* ddx_norm) {

  typename Derived::Scalar xdotx = x.squaredNorm();
  typename Derived::Scalar norm_x = std::sqrt(xdotx);
  x_norm = x / norm_x;

  if (dx_norm) {
    dx_norm->setIdentity(x.rows(), x.rows());
    (*dx_norm) -= x * x.transpose() / xdotx;
    (*dx_norm) /= norm_x;

    if (ddx_norm) {
      auto dx_norm_transpose = transposeGrad(*dx_norm, x.rows());
      auto ddx_norm_times_norm = -matGradMultMat(x_norm, x_norm.transpose(), (*dx_norm), dx_norm_transpose);
      auto dnorm_inv = -x.transpose() / (xdotx * norm_x);
      (*ddx_norm) = ddx_norm_times_norm / norm_x;
      auto temp = (*dx_norm) * norm_x;
      int n = x.rows();
      for (int col = 0; col < n; col++) {
        auto column_as_matrix = (dnorm_inv(0, col) * temp);
        for (int row_block = 0; row_block < n; row_block++) {
          ddx_norm->block(row_block * n, col, n, 1) += column_as_matrix.col(row_block);
        }
      }
    }
  }
}



// explicit instantiations
template Vector4d quat2axis(const MatrixBase<Vector4d>&);
template Matrix3d quat2rotmat(const MatrixBase<Vector4d>& q);
template Vector3d quat2rpy(const MatrixBase<Vector4d>&);

template Vector4d axis2quat(const MatrixBase<Vector4d>&);
template Matrix3d axis2rotmat(const MatrixBase<Vector4d>&);
template Vector3d axis2rpy(const MatrixBase<Vector4d>&);

template Vector4d rotmat2axis(const MatrixBase<Matrix3d>&);
template Vector4d rotmat2quat(const MatrixBase<Matrix3d>&);
template Vector3d rotmat2rpy(const MatrixBase<Matrix3d>&);

template Vector4d rpy2axis(const Eigen::MatrixBase<Vector3d>&);
template Vector4d rpy2quat(const Eigen::MatrixBase<Vector3d>&);
template Matrix3d rpy2rotmat(const Eigen::MatrixBase<Vector3d>&);

template Matrix<double, HomogeneousTransformSize, Dynamic> dHomogTrans(
    const Isometry3d&,
    const MatrixBase< Matrix<double, TwistSize, Dynamic> >&,
    const MatrixBase< MatrixXd >&);

template Matrix<double, HomogeneousTransformSize, Dynamic> dHomogTransInv(
    const Isometry3d&,
    const MatrixBase< Matrix<double, HomogeneousTransformSize, Dynamic> >&);

template typename Gradient< Matrix<double, TwistSize, Dynamic>, Dynamic, 1>::type dTransformAdjoint(
    const Isometry3d&,
    const MatrixBase< Matrix<double, TwistSize, Dynamic> >&,
    const MatrixBase< Matrix<double, HomogeneousTransformSize, Dynamic> >&,
    const MatrixBase<MatrixXd>&);

template typename Gradient< Matrix<double, TwistSize, Dynamic>, Dynamic, 1>::type dTransformAdjointTranspose(
    const Isometry3d&,
    const MatrixBase< Matrix<double, TwistSize, Dynamic> >&,
    const MatrixBase< Matrix<double, HomogeneousTransformSize, Dynamic> >&,
    const MatrixBase<MatrixXd>&);

template void normalizeVec(
    const MatrixBase< Vector3d >& x,
    Vector3d& x_norm,
    typename Gradient<Vector3d, 3, 1>::type* dx_norm = nullptr,
    typename Gradient<Vector3d, 3, 2>::type* ddx_norm = nullptr);

template void normalizeVec(
    const MatrixBase< Vector4d >& x,
    Vector4d& x_norm,
    typename Gradient<Vector4d, 4, 1>::type* dx_norm = nullptr,
    typename Gradient<Vector4d, 4, 2>::type* ddx_norm = nullptr);

