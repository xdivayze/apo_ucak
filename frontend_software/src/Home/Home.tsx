import { useEffect, useState } from "react";
import useGamepads from "./useGamepads";
import ConfigUI from "./ConfigUI";
import useGamepadAxes from "./useGamepadAxes";

export interface IAxis {
  index: number;
  value: number;
  description: string;
}

export default function Home() {
  const connectedGamepads = useGamepads();
  const [currentGamepad, setCurrentGamepad] = useState<Gamepad | null>(null);
  const [axisArr, setAxisArr] = useState<Array<IAxis>>([
    { index: 0, value: 0, description: "X" },
    { index: 1, value: 0, description: "Y" },
    { index: 2, value: 0, description: "Z" },
    { index: 3, value: 0, description: "rutter" },
  ]);
  useGamepadAxes(currentGamepad, axisArr, setAxisArr);

  

  const [configured, setConfigured] = useState(false);
  return (
    <div>
      {currentGamepad && (
        <div>
          {axisArr.map((v, i) => {
            return (
              <div key={i}>
                {v.description} : {v.value}
              </div>
            );
          })}
        </div>
      )}
      {!currentGamepad && (
        <div>
          no gamepads selected
          {connectedGamepads.map((g, i) => (
            <div
              onClick={() => {
                setCurrentGamepad(g);
              }}
            >
              {i} {g.id}
            </div>
          ))}
        </div>
      )}

      {currentGamepad && !configured && (
        <ConfigUI
          functionalityArray={axisArr}
          setFunctionalityArray={setAxisArr}
          setConfigured={setConfigured}
        />
      )}
    </div>
  );
}
