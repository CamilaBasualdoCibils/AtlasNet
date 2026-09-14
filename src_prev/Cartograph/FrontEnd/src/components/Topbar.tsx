export default function Topbar() {
  return (
    <header className="navbar navbar-expand-md navbar-dark d-print-none">
      <div className="container-xl">

        {/* LEFT: logo */}
        <div className="navbar-brand d-flex align-items-center gap-2">
          <img
            src="https://via.placeholder.com/32"
            className="rounded"
            width={32}
            height={32}
            alt="logo"
          />
          <span className="fw-bold">tabler</span>
        </div>

        {/* CENTER: menu (like Dashboards / Interface / etc.) */}
        <div className="navbar-nav flex-row gap-3 d-none d-md-flex">
          <a className="nav-link" href="#">Dashboards</a>
          <a className="nav-link" href="#">Interface</a>
          <a className="nav-link" href="#">Forms</a>
          <a className="nav-link" href="#">Extra</a>
          <a className="nav-link" href="#">Layout</a>
          <a className="nav-link" href="#">Help</a>
        </div>

        {/* RIGHT: icons */}
        <div className="navbar-nav flex-row ms-auto gap-2">

          {/* Sponsor */}
          <a className="nav-link" href="#">
            <span className="btn btn-outline-light btn-sm">
              ❤️ Sponsor
            </span>
          </a>

          {/* Theme icon */}
          <a className="nav-link" href="#">
            ☀️
          </a>

          {/* Notifications */}
          <a className="nav-link position-relative" href="#">
            🔔
            <span className="badge bg-red"></span>
          </a>

          {/* Language */}
          <a className="nav-link" href="#">
            EN
          </a>

          {/* Avatar */}
          <div className="nav-item dropdown">
            <a className="nav-link d-flex align-items-center" href="#">
              <img
                src="https://i.pravatar.cc/32"
                className="rounded-circle"
                width={32}
                height={32}
                alt="user"
              />
            </a>
          </div>

        </div>
      </div>
    </header>
  );
}