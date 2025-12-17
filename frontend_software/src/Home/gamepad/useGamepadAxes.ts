import { useEffect, useRef } from "react";

export const DEADZONE_DEFAULT = 0.002;
export const DEADZONE_RUDDER = 0.3;

export default function useGamepadAxes(
  gp: Gamepad | null,
  onAxesUpdate:(axes: readonly number[] )=>void,
) {
  const rafId = useRef<number>(0);

  useEffect(() => {
    const loop = () => {
      if (gp != null) {
        onAxesUpdate(gp.axes)
        
      }

      rafId.current = requestAnimationFrame(loop);
    };
    loop();

    return () => cancelAnimationFrame(rafId.current);
  }, [gp]);
}
