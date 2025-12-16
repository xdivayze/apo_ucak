import { useEffect, useState, type Dispatch, type SetStateAction } from "react";
import type { IAxis } from "./Home";

export default function ConfigUI({
  functionalityArray,
  setFunctionalityArray,
  setConfigured,
}: {
  functionalityArray: Array<IAxis>;
  setFunctionalityArray: Dispatch<SetStateAction<Array<IAxis>>>;
  setConfigured: Dispatch<SetStateAction<boolean>>;
}) {
  const [axisArr, setAxisArr] = useState(() =>
    functionalityArray.map((v) => ({ ...v, index: 0 }))
  );
  return (
    <div>
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
          setConfigured(true);
        }}
        className="text-white w-10 h-10 bg-blue-700"
      >
        Submit
      </div>
    </div>
  );
}
