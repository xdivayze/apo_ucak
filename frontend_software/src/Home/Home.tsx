import { useState } from "react";
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
    { index: 3, value: 0, description: "rudder" },
  ]);
  useGamepadAxes(currentGamepad, axisArr, setAxisArr);

  const [showConfigUI, setShowConfigUI] = useState(false);
  return (
    <div className="w-full h-full flex flex-col p-5  ">
      {currentGamepad && (
        <div
          onClick={() => {
            currentGamepad
              ? setShowConfigUI(!showConfigUI)
              : () => {
                  console.error(
                    "config ui cant be shown without gamepad selected"
                  );
                };
          }}
          className=" bg-gray-700 text-white flex justify-center h-7 w-1/6  rounded-md hover:cursor-pointer"
        >
          configure joystick
        </div>
      )}
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

      {currentGamepad && showConfigUI && (
        <ConfigUI
          functionalityArray={axisArr}
          setFunctionalityArray={setAxisArr}
          setShowConfigUI={setShowConfigUI}
        />
      )}
    </div>
  );
}
