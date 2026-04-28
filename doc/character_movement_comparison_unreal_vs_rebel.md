# CharacterMovementComponent Comparison

Scope:

- Unreal reference: root [CharacterMovementComponent.cpp](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp) and [CharacterMovementComponent.h](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.h)
- Custom implementation: [RebelEngine/src/Gameplay/Framework/CharacterMovementComponent.cpp](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp) and [RebelEngine/include/Engine/Gameplay/Framework/CharacterMovementComponent.h](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\include\Engine\Gameplay\Framework\CharacterMovementComponent.h)

Focus:

- locomotion architecture
- walking / falling
- floor finding
- collision resolution
- braking / friction

Excluded:

- networking
- root motion
- navmesh walking
- AI/path following

---

## 1. Executive Summary

The custom component is a compact kinematic controller built around:

- one floor probe
- one generic iterative sweep-and-slide solver
- one bespoke horizontal velocity model

Unreal's `CharacterMovementComponent` is a much more specialized locomotion framework. It separates walking, falling, floor finding, stepping, sliding, and braking into distinct algorithms, then coordinates them through sub-stepped movement loops.

The custom implementation already has some good qualities:

- clear state model
- grounded hysteresis
- decent braking substepping
- useful analog-input tuning
- simple and understandable collision flow

But compared with Unreal, it is missing the geometric robustness systems that make character locomotion stable in production environments:

- discrete step-up
- floor edge/perch handling
- walkable-vs-unwalkable surface classification during slide
- landing validation during falling
- richer floor traces
- mode-specific collision response

The shortest path to a large quality improvement is not to copy all of Unreal. It is to keep the custom velocity model and add Unreal-style geometry handling underneath it.

---

## 2. Architecture Comparison

### 2.1 Unreal

Unreal's architecture is mode-specific.

Walking path:

- [PhysWalking](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L5554)
- [CalcVelocity](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L3786)
- [MoveAlongFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L5457)
- [FindFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L7100)
- [StepUp](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L7450)
- [SlideAlongSurface](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L3572)

Falling path:

- [PhysFalling](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L4804)
- [CalcVelocity](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L3786)
- `NewFallVelocity`
- landing tests and slide deflection

Core architectural traits:

- mode-specific logic instead of one generic collision routine
- movement substeps per tick
- floor state as a first-class data structure
- explicit step traversal
- explicit landing validation
- post-move velocity reconstruction

### 2.2 RebelEngine custom implementation

Custom architecture:

- [PhysWalking](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L194)
- [PhysFalling](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L217)
- [FindFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L238)
- [MoveWithIterativeCollision](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L279)
- [CalcVelocity](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L523)
- [ApplyFalling](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L745)
- [UpdateMovementModeFromFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L785)

Core architectural traits:

- one main collision solver for both walking and falling
- one downward floor sweep
- mode transitions based mostly on floor distance + normal thresholds
- custom-tuned input and braking rules
- no dedicated stair or landing subsystems

### 2.3 Key architectural difference

Unreal is geometry-specialized.

The custom component is velocity-specialized.

That is the main difference. The custom controller spends more design effort on input feel and stop behavior. Unreal spends more design effort on contact classification and support management.

---

## 3. Side-by-Side System Table

| Area | Unreal | RebelEngine custom | Impact |
|---|---|---|---|
| Walking move pipeline | Dedicated `MoveAlongFloor` with ramp, step, and slide logic | Project desired delta onto floor plane, then generic collision solve | Simpler, but loses many walking-specific behaviors |
| Falling move pipeline | Midpoint integration, gravity split, landing validation, slope-boost protection | Euler-style integration and generic collision solve | More drift and less stable edge/landing behavior |
| Floor detection | Swept capsule, optional reuse, line-trace fallback, edge rejection, perch logic | Single downward capsule sweep | More false positives/negatives near edges and slopes |
| Step traversal | Explicit `StepUp` transaction | None | Stairs/curbs likely snag or act like walls |
| Surface classification | Walkable vs unwalkable affects movement response | Walkable mostly affects grounded state, not collision response | Steep slopes are less consistently treated as barriers |
| Sliding | Mode-specific `SlideAlongSurface` and `TwoWallAdjust` | Generic plane clipping + crease projection | Less control over wall/floor corner cases |
| Landing | `IsValidLandingSpot` + `FindFloor` on impact | Ground state updates after move from floor probe | Less precise landing on edges and complex contacts |
| Braking | Generalized friction + braking model | Highly tuned analog, reversal, slope, and overspeed handling | Custom feel tuning is stronger here |
| Substepping | Full movement loop substeps plus braking substeps | Braking substeps only | More sensitive to larger frame times |
| Robustness tools | Perching, edge tolerance, slope boost suppression, floor cache checks | Hysteresis, snap, bounded depenetration | Good base, but much narrower protection envelope |

---

## 4. Missing Features

### 4.1 Discrete step-up

Missing custom equivalent to Unreal's [StepUp](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L7450).

What this means:

- small curbs become blocking walls
- staircases require slope proxies or oversized ramps
- step-height traversal is not controllable as a gameplay parameter

### 4.2 Dedicated walking collision path

Unreal has [MoveAlongFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L5457), which can:

- continue onto another walkable ramp
- step over a barrier
- slide only after walk-specific options fail

The custom code has no walking-specific collision resolver. It only has [MoveWithIterativeCollision](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L279).

### 4.3 Rich floor query pipeline

Missing from the custom `FindFloor`:

- shrunken sweep shapes
- line-trace fallback
- edge tolerance rejection
- reduced-radius perch test
- supplied downward-sweep reuse
- penetration-aware floor interpretation

Unreal's floor work lives in [ComputeFloorDist](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L6949) and [FindFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L7100).

### 4.4 Landing validation

Missing custom equivalent to Unreal's falling landing path in [PhysFalling](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L4804), especially:

- `IsValidLandingSpot`
- floor conversion after edge hits
- immediate mode switch on valid landing

### 4.5 Walking-specific slide rules

Missing custom equivalent to Unreal's [SlideAlongSurface](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L3572), which:

- flattens normals for steep unwalkable slopes while grounded
- avoids pushing downward into the floor
- preserves legal support constraints while sliding

### 4.6 Two-wall adjustment rules

The custom solver handles corners by projecting onto the crease direction in [MoveWithIterativeCollision](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L325). Unreal's [TwoWallAdjust](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\CharacterMovementComponent.cpp#L3612) adds walking-aware upward allowance and clamping.

### 4.7 Full movement substepping

The custom component substeps braking, but not the entire locomotion solve. Unreal substeps movement loops themselves in walking and falling.

Impact:

- more frame-rate sensitivity
- rougher collision behavior on large `deltaTime`
- less stable high-speed edge interactions

---

## 5. Algorithmic Differences

### 5.1 Walking

Unreal:

```text
PhysWalking
  -> maintain horizontal ground velocity
  -> CalcVelocity
  -> MoveAlongFloor
      -> ramp projection
      -> step attempt
      -> slide fallback
  -> FindFloor / step-down floor reuse
  -> adjust floor height and base
  -> derive final velocity from actual motion
```

Custom:

```text
PhysWalking
  -> CalcVelocity
  -> project desired delta to floor plane
  -> generic iterative collision solve
  -> derive XY velocity from actual motion
  -> zero Z velocity
```

Custom walking is simpler and easier to reason about, but it does not distinguish between:

- ramp continuation
- step candidate
- barrier
- ledge
- invalid support

Unreal does.

### 5.2 Falling

Unreal:

- computes lateral motion separately from gravity
- uses midpoint integration
- validates landing spots on impacts
- limits slope boosting on slide
- has special handling for apex timing

Custom:

- applies acceleration and gravity directly in [ApplyFalling](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L745)
- moves by `Velocity * dt`
- updates grounded state afterward

Result:

- custom code is fine for basic jump/fall motion
- Unreal is much more stable around edges, ceilings, and sloped impacts

### 5.3 Collision response

Unreal collision response is semantic:

- classify hit
- choose floor/ramp/step/slide/land behavior

Custom collision response is geometric:

- move
- clip
- retry
- crease-project if needed

The custom approach is valid, but it leaves a lot of walking behavior implicit. Unreal makes those decisions explicit.

### 5.4 Floor state

Custom floor state:

- blocking hit
- walkable flag
- normal
- distance

Unreal floor state is richer in behavior even if conceptually similar:

- sweep vs line trace
- walkable support interpretation
- perch reinterpretation
- penetration-aware handling
- cache reuse rules

This richer floor model is a major reason Unreal behaves better on ledges and near-threshold slopes.

### 5.5 Braking and input

This is one area where the custom implementation is not merely “missing” Unreal behavior. It has its own strengths.

Custom `CalcVelocity` adds:

- dead-zone remapping
- analog minimum control floors
- active lateral slip cleanup
- reversal-specific braking
- slope-aware no-input braking scales
- overspeed damping while input remains active

Unreal's braking model is cleaner and more generic, but less tuned to the exact feel cases the custom code is already targeting.

Conclusion:

- keep the custom velocity feel model
- improve the geometry/contact model around it

---

## 6. Robustness Differences

### 6.1 Areas where Unreal is stronger

Unreal is materially more robust in:

- stairs and curbs
- transitions between ramp and wall
- ledge edges
- landing on sloped or edge-adjacent surfaces
- shallow penetrations and ambiguous contacts
- large-frame-time motion stability
- steep-slope walking rejection

### 6.2 Why Unreal is stronger

Not because of one big algorithm. Because of many small guardrails:

- edge tolerance checks
- reduced-radius perch tests
- floor sweep shrinkage
- line-trace fallback
- stepping as a transaction
- slide normal rewriting
- two-wall special cases
- slope boost suppression
- floor cache invalidation rules
- mode-specific movement loops

### 6.3 Areas where the custom implementation is already good

The custom component already has useful robustness work in:

- grounded hysteresis in [UpdateMovementModeFromFloor](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L785)
- snap-to-ground in [TryApplyGroundSnap](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L486)
- bounded depenetration in [ResolvePenetration](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L419)
- corner crease projection in [MoveWithIterativeCollision](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L325)
- frame-rate-resistant braking through substeps in [CalcVelocity](C:\Users\umutu\Perforce\perforce_Umut_6700\RebelEngine\RebelEngine\src\Gameplay\Framework\CharacterMovementComponent.cpp#L523)

These are good foundations. The issue is not that the custom controller is naive. The issue is that its robustness work is concentrated in velocity feel and generic collision, while Unreal concentrates it in contact classification and support logic.

### 6.4 Likely failure modes in the custom implementation

Likely problem cases:

- capsule catches on stair risers
- walkable ledge edges incorrectly switch to falling or remain grounded
- steep slopes may slide or stick inconsistently because walkability affects mode more than slide normals
- edge-adjacent landings may not convert cleanly from falling to walking
- single large `deltaTime` may produce harsher cornering/contact artifacts than expected

---

## 7. Potential Improvements

## 7.1 Highest-value improvements

### A. Add `StepUp`

Priority: highest

Why:

- largest gameplay payoff
- immediately improves stairs, curbs, and small barriers
- removes the need to fake stairs with ramps

Recommended shape:

- copy Unreal's transaction model conceptually
- do not copy the full code wholesale
- implement:
  - step-up precheck
  - upward move
  - forward move
  - downward settle
  - support validation
  - revert on failure

### B. Add a real `MoveAlongFloor`

Priority: highest

Why:

- walking should not go through the same collision path as falling
- this is where ramp continuation, step attempts, and slide fallback belong

Recommended shape:

- for walking only:
  - compute floor-constrained move
  - attempt move
  - if hit walkable ramp, continue on ramp
  - if hit barrier, try `StepUp`
  - else slide with walking-specific normals

### C. Upgrade `FindFloor`

Priority: highest

Why:

- the current single sweep is the biggest geometry robustness bottleneck

Recommended additions:

- shrunken capsule sweep
- edge tolerance rejection
- line trace fallback
- optional reduced-radius perch test
- explicit “started penetrating” handling

## 7.2 Medium-value improvements

### D. Split slide behavior by mode

Priority: medium

Walking slide should:

- flatten steep upward normals
- avoid pushing into the floor

Falling slide should:

- clamp upward boost after impacts

### E. Add landing validation on falling impacts

Priority: medium

On falling collision:

- test if hit is a valid landing spot
- if ambiguous, run a floor query from the impact position
- switch to walking immediately when valid

### F. Add full movement substeps

Priority: medium

Apply substeps not just to braking, but to walking/falling movement integration itself.

Benefits:

- better stability on low frame rates
- more predictable impact resolution
- less tunneling-like motion error for controller movement

## 7.3 Lower-cost improvements

### G. Enrich floor state

Add to `FloorResult`:

- bStartPenetrating
- bLineTrace
- bWithinEdgeTolerance
- optional perch result
- optional cached-valid flag

### H. Preserve custom velocity feel model

Do not replace the current `CalcVelocity` with Unreal's generic one.

Keep:

- analog tuning
- active slip cleanup
- slope-aware no-input braking
- overspeed correction

Those are useful differentiators.

### I. Rebuild walking velocity from actual floor-relative displacement

Current walking writes:

- solved velocity
- then XY from actual displacement
- then `Z = 0`

That is acceptable for a Z-up game, but if slope motion gets richer, it is cleaner to derive velocity relative to support constraints rather than hard-zeroing Z every frame.

---

## 8. Recommended Upgrade Roadmap

### Phase 1: geometry correctness

1. Implement `MoveAlongFloor`.
2. Implement `StepUp`.
3. Upgrade `FindFloor` to sweep + line fallback + edge rejection.

Expected result:

- stairs work
- ledge behavior improves
- walk/fall transitions stop flapping as often

### Phase 2: contact robustness

1. Add mode-specific `SlideAlongSurface`.
2. Add landing validation during falling.
3. Add richer floor metadata.

Expected result:

- fewer bad edge landings
- less sticky steep-slope behavior
- more stable compound contacts

### Phase 3: integration stability

1. Add sub-stepped walking/falling loops.
2. Add explicit support-state debug visualization and counters.
3. Add regression tests for steps, ledges, ramps, and corner impacts.

Expected result:

- better low-FPS stability
- easier diagnosis of remaining contact bugs

---

## 9. Suggested Target Architecture

The best next architecture for the custom component is:

```text
Tick
  -> FindFloor
  -> UpdateMode
  -> if Walking:
         CalcVelocity (keep current custom version)
         MoveAlongFloor
         FindFloor
         GroundSnap / adjust support
     if Falling:
         ApplyFalling
         Sweep move
         if impact:
             check landing
             else slide using falling rules
```

Key principle:

- keep the custom speed/response model
- replace the generic contact model for walking

That gets most of Unreal's robustness benefits without turning the custom controller into a direct port.

---

## 10. Bottom Line

The custom controller is already a decent movement-feel implementation. It is not yet a robust locomotion geometry implementation.

Unreal's advantage is not mainly better acceleration math. It is better contact semantics:

- what counts as floor
- what counts as a valid landing
- when to step instead of slide
- when to flatten a normal
- when an edge is still supportable

If you only implement one thing next, implement `StepUp`.

If you implement three things next, implement:

1. `StepUp`
2. `MoveAlongFloor`
3. a stronger `FindFloor`

That is the highest-return path.
