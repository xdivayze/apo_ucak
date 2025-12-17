import { useState } from "react";
import useGamepads, { joystickMapping } from "./gamepad/useGamepads";
import ConfigUI from "./gamepad/ConfigUI";
import useGamepadAxes from "./gamepad/useGamepadAxes";

export interface IAxis {
  index: number;
  value: number;
  description: string;
  deadzone: number;
}

export default function Home() {
  const connectedGamepads = useGamepads();
  const [currentGamepad, setCurrentGamepad] = useState<Gamepad | null>(null);
  const [axisArr, setAxisArr] = useState<Array<IAxis>>(joystickMapping);
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
                console.log(g.axes.length);
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
