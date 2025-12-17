import { useState, type Dispatch, type SetStateAction } from "react";
import type { IAxis } from "../Home";

export default function ConfigUI({
  functionalityArray,
  setFunctionalityArray,
  setShowConfigUI,
}: {
  functionalityArray: Array<IAxis>;
  setFunctionalityArray: Dispatch<SetStateAction<Array<IAxis>>>;
  setShowConfigUI: Dispatch<SetStateAction<boolean>>;
}) {
  const [axisArr, setAxisArr] = useState(() =>
    functionalityArray.map((v) => ({ ...v, index: 0 }))
  );
  return (
    <div className="flex w-full h-full flex-col justify-center">
      <div>
        {axisArr.map((v, i) => (
          <div key={i}>
            {v.description}:
            <select
              onChange={(e) => {
                const idx = Number(e.target.value);
                setAxisArr((prev) =>
                  prev.map((item, j) =>
                    j === i ? { ...item, index: idx } : item
                  )
                );
              }}
            >
              {axisArr.map((_, i) => (
                <option value={i}>{i} </option>
              ))}
            </select>
          </div>
        ))}
      </div>
      <div
        onClick={() => {
          setFunctionalityArray(axisArr);
          setShowConfigUI(false);
        }}
        className="text-white w-1/6 h-6 rounded-md justify-center flex flex-col text-center hover:cursor-pointer bg-blue-700"
      >
        Submit
      </div>
    </div>
  );
}
