
#ifndef POINT_H_
#define POINT_H_

#include <QPoint>
#include <cstdint>
#include <QtMath>

class Point {
public:
    constexpr Point() : Point(0, 0) {}
    constexpr Point(int64_t x, int64_t y) : x_(x), y_(y) {}

public:
    static Point fromQPoint(const QPoint &p) {
        return Point(p.x(), p.y());
    }

    constexpr QPoint toQPoint() const {
        return QPoint(
                    static_cast<int>(x_),
                    static_cast<int>(y_)
                    );
    }

public:
    constexpr bool isNull() const { return x_ == 0 && y_ == 0; }

    constexpr int64_t x() const    { return x_; }
    constexpr int64_t y() const    { return y_; }
    constexpr void setX(int64_t x) { x_ = x; }
    constexpr void setY(int64_t y) { y_ = y; }

    constexpr int64_t manhattanLength() const { return qAbs(x_) + qAbs(y_); }

    constexpr int64_t &rx() { return x_; }
    constexpr int64_t &ry() { return y_; }

    constexpr Point &operator+=(const Point &p) { x_ += p.x_; y_ += p.y_; return *this; }
    constexpr Point &operator-=(const Point &p) { x_ -= p.x_; y_ -= p.y_; return *this; }

    constexpr Point &operator*=(float factor)   { x_ = qRound(x_ * factor); y_ = qRound(y_ * factor); return *this; }
    constexpr Point &operator*=(double factor)  { x_ = qRound(x_ * factor); y_ = qRound(y_ * factor); return *this; }
    constexpr Point &operator*=(int64_t factor) { x_ = x_ * factor; y_ = y_ * factor; return *this; }

    constexpr Point &operator/=(qreal divisor) {
        x_ = qRound(x_ / divisor);
        y_ = qRound(y_ / divisor);
        return *this;
    }

    constexpr static int64_t dotProduct(const Point &p1, const Point &p2) { return p1.x_ * p2.x_ + p1.y_ * p2.y_; }


public:
    friend constexpr bool operator==(const Point &p1, const QPoint &p2)       { return p1.x_ == p2.x() && p1.y_ == p2.y(); }
    friend constexpr bool operator==(const QPoint &p1, const Point &p2)       { return p1.x() == p2.x_ && p1.y() == p2.y_; }
    friend constexpr bool operator!=(const Point &p1, const QPoint &p2)       { return p1.x_ != p2.x() || p1.y_ != p2.y(); }
    friend constexpr bool operator!=(const QPoint &p1, const Point &p2)       { return p1.x() != p2.x_ || p1.y() != p2.y_; }

public:
    friend constexpr bool operator==(const Point &p1, const Point &p2)       { return p1.x_ == p2.x_ && p1.y_ == p2.y_; }
    friend constexpr bool operator!=(const Point &p1, const Point &p2)       { return p1.x_ != p2.x_ || p1.y_ != p2.y_; }
    friend constexpr const Point operator+(const Point &p1, const Point &p2) { return Point(p1.x_+p2.x_, p1.y_+p2.y_); }
    friend constexpr const Point operator-(const Point &p1, const Point &p2) { return Point(p1.x_-p2.x_, p1.y_-p2.y_); }
    friend constexpr const Point operator*(const Point &p, float factor)     { return Point(qRound(p.x_ * factor), qRound(p.y_ * factor)); }
    friend constexpr const Point operator*(float factor, const Point &p)     { return Point(qRound(p.x_ * factor), qRound(p.y_ * factor)); }
    friend constexpr const Point operator*(const Point &p, double factor)    { return Point(qRound(p.x_ * factor), qRound(p.y_ * factor)); }
    friend constexpr const Point operator*(double factor, const Point &p)    { return Point(qRound(p.x_ * factor), qRound(p.y_ * factor)); }
    friend constexpr const Point operator*(const Point &p, int64_t factor)   { return Point(p.x_ * factor, p.y_ * factor); }
    friend constexpr const Point operator*(int64_t factor, const Point &p)   { return Point(p.x_ * factor, p.y_ * factor); }
    friend constexpr const Point operator+(const Point &p)                   { return p; }
    friend constexpr const Point operator-(const Point &p)                   { return Point(-p.x_, -p.y_); }
    friend constexpr const Point operator/(const Point &p, qreal c)          { return Point(qRound(p.x_ / c), qRound(p.y_ / c)); }

private:
    int64_t x_;
    int64_t y_;
};

Q_DECLARE_TYPEINFO(Point, Q_MOVABLE_TYPE);

#endif
