import { useEffect, useState } from "react";

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
