import { Routes, Route } from "react-router-dom";
import Navbar from "./components/Navbar";

import Dashboard from "./pages/Dashboard";
import Map from "./pages/Map";

export default function App() {
  return (
    <>
      <Navbar />
      <div className="container-xl mt-3">
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/map" element={<Map />} />

        </Routes>
      </div>
    </>
  );
}