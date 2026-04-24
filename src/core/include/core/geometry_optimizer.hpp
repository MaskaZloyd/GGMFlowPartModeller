#pragma once

#include "core/area_profile.hpp"
#include "core/error.hpp"
#include "core/flow_solver.hpp"
#include "core/geometry.hpp"
#include "core/pump_params.hpp"
#include "core/target_area_curve.hpp"

#include <span>
#include <vector>

namespace ggm::core {

struct GeometryDesignBounds
{
  double dinMin{0.0};
  double dinMax{0.0};

  double b2Min{0.0};
  double b2Max{0.0};

  double xaMin{0.0};
  double xaMax{0.0};

  double r1Min{0.0};
  double r1Max{0.0};

  double r2Min{0.0};
  double r2Max{0.0};

  double r3Min{0.0};
  double r3Max{0.0};

  double r4Min{0.0};
  double r4Max{0.0};

  double al1DegMin{-30.0};
  double al1DegMax{30.0};

  double al2DegMin{-30.0};
  double al2DegMax{30.0};

  double al02DegMin{-30.0};
  double al02DegMax{30.0};

  double be1DegMin{5.0};
  double be1DegMax{85.0};

  double be3RawDegMin{5.0};
  double be3RawDegMax{85.0};
};

// Per-variable opt-in mask. Each flag selects whether the corresponding
// PumpParams field is moved by the optimizer or kept at its imported
// value. D₂ and Dvt are NEVER optimized (treated as fixed inputs from the
// meridional module) so they have no flag here.
struct GeometryVariableMask
{
  bool din{ false };
  bool b2{ true };
  bool r1{ true };
  bool r2{ true };
  bool r3{ true };
  bool r4{ true };
  bool al1{ false };
  bool al2{ false };
  bool al02{ false };
  bool be1{ false };
  bool be3{ false };
};

struct GeometryOptimizationSettings
{
  int sampleCount{ 80 };

  // Max CMA-ES generations. Each generation samples λ candidates.
  int maxGenerations{ 100 };

  // Override for the CMA-ES population size (λ). 0 means "auto" → the
  // classical default `4 + ⌊3·ln N⌋`.
  int populationSize{ 0 };

  // Initial step-size σ, measured as a fraction of each coordinate's
  // `[min, max]` range (0.3 ≈ 30 % of the box per dimension).
  double sigmaInitial{ 0.3 };

  double areaWeight{ 1.0 };
  double smoothnessWeight{ 1e-3 };
  double constraintWeight{ 1e3 };

  double maxInvalidChordFraction{ 0.20 };

  // Absolute-area anchor. When > 0, areaError is computed against target
  // values scaled by this reference (typically π·D₂·b₂ from the initial
  // params), so the optimizer cannot cheat by producing a geometrically-
  // similar but mis-sized candidate. When 0 (default), areaError is shape-only.
  double referenceOutletArea{ 0.0 };

  GeometryVariableMask mask{};

  unsigned int seed{ 42 };

  bool useFemValidation{ true };
};

struct GeometryObjectiveBreakdown
{
  double total{0.0};
  double areaError{0.0};
  double smoothnessPenalty{0.0};
  double constraintPenalty{0.0};
  bool valid{false};
};

struct GeometryOptimizationResult
{
  PumpParams params;
  MeridionalGeometry geometry;
  AreaProfile areaProfile;

  GeometryObjectiveBreakdown objective;

  bool converged{false};
  int generations{0};
};

// Optimizable design variable: points into PumpParams for the value,
// GeometryDesignBounds for the lower/upper limits, GeometryVariableMask for
// the enable flag, plus a display label for UI.
struct DesignVariable
{
  const char* label;
  double PumpParams::* param;
  double GeometryDesignBounds::* minBound;
  double GeometryDesignBounds::* maxBound;
  bool GeometryVariableMask::* enabled;
};

// Canonical list of all variables the optimizer can potentially move.
// Each variable carries its own mask flag — the optimizer only moves those
// whose flag is true. D₂, Dvt, din, xa are never optimized.
[[nodiscard]] std::span<const DesignVariable>
allDesignVariables() noexcept;

// Legacy helper: generic wide bounds. Prefer makeBoundsFromValues(params)
// which anchors each bound to ±50 % of the imported value.
[[nodiscard]] GeometryDesignBounds
makeDefaultBounds(double d2, double dvt);

// Default bounds as ±50 % of each current parameter value. Signed params
// (angles) use the symmetric interval [v − 0.5·|v|, v + 0.5·|v|] so the
// sign is preserved. D₂/Dvt bounds are set to ±50 % too for completeness,
// but they are not optimized in practice.
[[nodiscard]] GeometryDesignBounds
makeBoundsFromValues(const PumpParams& params) noexcept;

[[nodiscard]] Result<std::vector<double>>
normalizedAreaSamples(const AreaProfile& profile, int sampleCount);

[[nodiscard]] GeometryObjectiveBreakdown
evaluateGeometryObjective(const PumpParams& params,
                          const TargetAreaCurve& target,
                          const GeometryOptimizationSettings& settings);

[[nodiscard]] Result<GeometryOptimizationResult>
evaluateGeometryCandidate(const PumpParams& params,
                          const TargetAreaCurve& target,
                          const GeometryOptimizationSettings& settings);

[[nodiscard]] Result<GeometryOptimizationResult>
optimizeGeometryForTargetArea(const PumpParams& initialParams,
                              const TargetAreaCurve& target,
                              const GeometryDesignBounds& bounds,
                              const GeometryOptimizationSettings& settings,
                              const CancelPredicate& isCancelled = {});

} // namespace ggm::core
