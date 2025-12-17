import { useEffect, useRef, type Dispatch, type SetStateAction } from "react";
import type { IAxis } from "./Home";

export const DEADZONE_DEFAULT = 0.002;
export const DEADZONE_RUDDER = 0.3;

export default function useGamepadAxes(
  gp: Gamepad | null,
  axisValues: Array<IAxis>,
  setAxisValues: Dispatch<SetStateAction<Array<IAxis>>>
) {
  const rafId = useRef<number>(0);
  const axisArrRef = useRef(axisValues);
  useEffect(() => {
    axisArrRef.current = axisValues;
  }, axisValues);
  useEffect(() => {
    const loop = () => {
      if (gp != null) {
        const mapping = axisArrRef.current;

        setAxisValues((prev) =>
          prev.map((item, i) => {
            const val = gp.axes[mapping[i]?.index ?? 0] ?? 0;
            return {
              ...item,
              value: Math.abs(val) >= item.deadzone ? val : 0,
            };
          })
        );
      }

      rafId.current = requestAnimationFrame(loop);
    };
    loop();

    return () => cancelAnimationFrame(rafId.current);
  }, [gp]);
}
