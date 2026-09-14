import Mapview from "../components/Mapview.tsx";

export default function Map() {
  return (
    <div className="page">
    
      <div className="page-wrapper">
        <div className="page-body">
          <div className="container-xl">
            <div className="row row-deck row-cards">


              <div className="col-12">
                <div className="card">
                  <div
                    className="card-body"
                    style={{ height: "40rem" }}
                    
                  ><Mapview /></div>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}