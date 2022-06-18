#pragma once

#include <cmath>
#include <utility>

namespace chiralscroll
{

// A 2-dimensional vector.
template<typename T>
class Vector
{
public:
	constexpr Vector(T x, T y) : x_(x), y_(y) {}

	template<typename U>
	friend class Vector;

	template<typename U>
	using Result = decltype(std::declval<T>() + std::declval<U>());

	friend constexpr Vector operator+(Vector v)
	{
		return v;
	}
	template<typename U>
	constexpr Vector& operator+=(Vector<U> rhs)
	{
		x_ += rhs.x_;
		y_ += rhs.y_;
		return *this;
	}
	template<typename U>
	friend constexpr Vector<Result<U>> operator+(Vector lhs, Vector<U> rhs)
	{
		return Vector<Result<U>>(lhs.x_ + rhs.x(), lhs.y_ + rhs.y());
	}

	friend constexpr Vector operator-(Vector v)
	{
		return Vector(-v.x_, -v.y_);
	}
	template<typename U>
	constexpr Vector& operator-=(Vector<U> rhs)
	{
		x_ -= rhs.x_;
		y_ -= rhs.y_;
		return *this;
	}
	template<typename U>
	friend constexpr Vector<Result<U>> operator-(Vector lhs, Vector<U> rhs)
	{
		return Vector<Result<U>>(lhs.x_ - rhs.x(), lhs.y_ - rhs.y());
	}

	// Dot product.
	template<typename U>
	friend constexpr Result<U> operator*(Vector lhs, Vector<U> rhs)
	{
		return lhs.x_*rhs.x() + lhs.y_*rhs.y();
	}

	// Scalar product.
	template<typename U>
	friend constexpr Vector<Result<U>> operator*(Vector lhs, U rhs)
	{
		return Vector<Result<U>>(lhs.x_*rhs, lhs.y_*rhs);
	}
	template<typename U>
	friend constexpr Vector<Result<U>> operator*(U lhs, Vector rhs)
	{
		return Vector<Result<U>>(lhs*rhs.x_, lhs*rhs.x_);
	}
	template<typename U>
	friend constexpr Vector<Result<U>> operator/(Vector lhs, U rhs)
	{
		return Vector<Result<U>>(lhs.x_/rhs, lhs.y_/rhs);
	}

	friend constexpr bool operator==(Vector lhs, Vector rhs)
	{
		return lhs.x_ == rhs.x_ && lhs.y_ == rhs.y_;
	}
	friend constexpr bool operator!=(Vector lhs, Vector rhs)
	{
		return !(lhs == rhs);
	}

	// Norm (magnitude) squared.
	constexpr T Norm2() const
	{
		return x_*x_ + y_*y_;
	}

	// Norm (magnitude).
	constexpr double Norm() const
	{
		return sqrt(Norm2());
	}

	constexpr T x() const
	{
		return x_;
	}

	constexpr T y() const
	{
		return y_;
	}

private:
	T x_;
	T y_;
};

}  // namespace chiralscroll
