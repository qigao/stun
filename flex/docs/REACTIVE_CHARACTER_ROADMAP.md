# Flex Engine: The 2.5D Virtual Production Studio
**Focus:** Real-time character performance, physics-driven world interaction, and deterministic simulation recording.

---

## 0. The Philosophy: Filming the Simulation
Flex is not a video player; it is a **Real-Time Kinematic Laboratory**. In this engine, we don't "animate" characters—we "perform" them inside a physically valid world.

1.  **Physics as the Arbiter**: The DSL defines the terrain and physical limits. Performance (AI or Human) is a "request" that the engine validates against the world's physics (Physics-over-Performance).
2.  **Deterministic DNA**: Every take is recorded as a stream of inputs and state changes (The DNA), not pixels. This allows for infinite "re-shoots" with different camera lenses, lighting, and resolution.
3.  **The 2.5D Depth Continuum**: Leveraging vector parallax, Z-depth occlusion, and "fleshy" mesh deformation to create a 3D sense of space using 2D assets.

---

## 1. The Reactive Core (The "Pinocchio Pattern")
The visual appearance of a character is a direct, reactive function of its internal state.
*   **Hierarchical Structure**: `Body` -> `Head` -> `Eyes`. Transformations propagate with local origins.
*   **Multi-Track Animations**: A single semantic input (e.g., `sadness: 0.8`) simultaneously drives `#mouth/morph`, `#brow/y`, and `#eyes/filter`.
*   **State Machine Bridge**: A `machine` translates semantic inputs into animation playback, handling transitions and layering.

## 2. The Physical World (IK & Box2D v3)
Characters inhabit a world with boundaries, weight, and mechanical constraints.
*   **Declarative IK**: Introduces `bone` and `ik_target` keywords. Limb movement is solved via Analytical (2-bone) or CCD solvers.
*   **Integrated Physics**: Designers define `physics` blocks directly on nodes.
    ```flex
    rect crate {
        physics { body: dynamic; density: 1.2; friction: 0.5; }
    }
    ```
*   **Mechanical Gizmos**: Native support for Joints (Distance, Prismatic, Revolute) to build complex machinery or interactive UI.
*   **Raycast Vision**: AI nodes use `vision_ray` to react to the environment and other characters.

## 3. The Expressive Character (Pro-Cartoon Grade)
Bridging the gap between rigid shapes and fluid, biological motion.
*   **Fourier-Kalman Morphing**: Decompose SVG paths into frequency harmonics for seamless morphing between mismatched point counts. A Kalman Filter tracks these harmonics to smooth camera jitter and provide automatic point reduction.
*   **Vitality & Autonomic Systems**: Declarative `vitality` block for involuntary life signs: `breath_rate`, `blink_rate`, and `micro_tremor`.
*   **Procedural Drag**: A `drag` property on child nodes (ears, hair, tails) creates automatic "Follow Through" and secondary motion.
*   **Viseme Blending**: Built-in cross-fading between phoneme targets for natural, automated speech.

## 4. 2.5D Cinematography (The Camera & Depth)
Managing the "Lens" and the sense of three-dimensional space.
*   **The Director’s Camera**: A `camera` node supporting `zoom`, `dolly`, and `focus_node` (automated depth-of-field tracking).
*   **Z-Depth Parallax**: Automatic calculation of scale and atmospheric speed based on a node's `z_index`, creating a real-time landscape depth effect.
*   **Dynamic Shadows**: A global `light_source` that projects "drop silhouettes" and calculates backlighting effects (Subsurface Scattering).
*   **Layer Swapping**: The state machine handles complex occlusion by animating `z_index` and active layers.

## 5. Virtual Production: Actor-in-the-Loop
Real-time "Puppetry" where a human actor drives the character via webcam or tracking device.
*   **Physics-over-Performance**: If an actor reaches for an object but a wall is in the way, the Physics Arbiter stops the arm. The character is physically "present" in the scene.
*   **Take Management (.flex-take)**: Record a "Performance" as a deterministic stream of inputs. Re-shoot the scene later with higher resolution or different camera angles without needing the actor.
*   **Input Priority Masking**: Resolve conflicts between recorded keyframes and live camera data using weighted priority.

## 6. The Production Pipeline (Studio-Scale)
Scaling from a single character to a full cinematic production.
*   **Deterministic Ticking**: Every simulation step is locked to a global `uint64_t` tick index, ensuring bit-perfect replication across local previews and remote farms.
*   **Massive Crowds (Scatter Nodes)**: GPU-instanced "Scatter" nodes for thousands of agents with unique animation offsets (Behavorial Jitter).
*   **Blender Integration**: A custom Python plugin to export Blender Armatures and F-Curve keyframes directly into the `.flex` format.
*   **The Render Farm**: Headless, distributed CLI rendering with deterministic time-stepping across clusters.

---

## 7. The Technical Core (System Stability)
The mathematical hurdles required to stabilize the "Studio" experience.
*   **Pixels-to-Meters Layer**: Internal translation between DSL pixels (e.g., 50px = 1m) and Box2D v3 meters.
*   **Dual-Quaternion Skinning**: Deformable vector meshes to prevent the "candy-wrapper" effect at joints.
*   **Async Performance Capture Sync**: Double-buffered input tracking with 20ms "Look-Back" buffers to eliminate tracking stutter.
*   **Global Material Transitions**: Material property overrides in the state machine for global character changes (e.g., Water-to-Ice).

---

## 8. Next Steps: Building the Studio
1.  **[CORE] Deterministic Tick Index**: Refactor `Instance::advance` to use a globally synchronized tick index instead of wall-clock time.
2.  **[IO] Simulation Recorder**: Implement the capture/replay system for `.flex-take` input streams.
3.  **[MATH] Fourier-Kalman Solver**: Build the frequency-domain path interpolator and the harmonic Kalman smoothing layer.
4.  **[RENDER] 2.5D Depth Engine**: Implement automated Z-index-to-Parallax and Scale/Depth logic.
5.  **[PHYSICS] Conflict Resolver**: Create the "Physics-over-Performance" arbiter to handle IK/Collision conflicts.
