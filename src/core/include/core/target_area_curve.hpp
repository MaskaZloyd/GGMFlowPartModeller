#pragma once

#include <span>
#include <vector>

namespace ggm::core {

struct TargetAreaPoint
{
  double xi{0.0};
  double value{0.0};
};

class TargetAreaCurve
{
public:
  TargetAreaCurve();

  explicit TargetAreaCurve(std::vector<TargetAreaPoint> points);

  [[nodiscard]] std::span<const TargetAreaPoint> points() const noexcept;
  [[nodiscard]] std::span<TargetAreaPoint> points() noexcept;

  void setPoints(std::vector<TargetAreaPoint> points);

  [[nodiscard]] double evaluate(double xi) const noexcept;

  void sortAndClamp();

  [[nodiscard]] bool isValid() const noexcept;

private:
  std::vector<TargetAreaPoint> points_;
};

}
