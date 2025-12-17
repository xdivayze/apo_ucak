import { useEffect, useState } from "react";
import type { IAxis } from "../Home";
import { DEADZONE_DEFAULT, DEADZONE_RUDDER } from "./useGamepadAxes";

export const joystickMapping: Array<IAxis> = [
    { index: 0, value: 0, description: "X", deadzone: DEADZONE_DEFAULT },
    { index: 1, value: 0, description: "Y", deadzone: DEADZONE_DEFAULT },
    { index: 6, value: 0, description: "gaz", deadzone: DEADZONE_DEFAULT },
    { index: 5, value: 0, description: "rudder", deadzone: DEADZONE_RUDDER },
  ]

  export const standardMapping: Array<IAxis> = [
    { index: 0, value: 0, description: "X", deadzone: DEADZONE_DEFAULT },
    { index: 1, value: 0, description: "Y", deadzone: DEADZONE_DEFAULT },
    { index: 2, value: 0, description: "gaz", deadzone: DEADZONE_DEFAULT },
    { index: 3, value: 0, description: "rudder", deadzone: DEADZONE_RUDDER },
  ]

export default function useGamepads() {
  const [connected, setConnected] = useState<Gamepad[]>([]);
  useEffect(() => {
    const refreshList = () => {
      const pads = Array.from(navigator.getGamepads()).filter(
        (g): g is Gamepad => !!g
      );
      setConnected(pads);
    };

    window.addEventListener("gamepadconnected", refreshList);
    window.addEventListener("gamepaddisconnected", refreshList);

    refreshList();

    return () => {
      window.removeEventListener("gamepadconnected", refreshList);
      window.removeEventListener("gamepaddisconnected", refreshList);
    };
  }, []);

  return connected;
}
