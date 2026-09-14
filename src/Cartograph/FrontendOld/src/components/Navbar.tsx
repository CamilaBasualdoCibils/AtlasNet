export default function Navbar() {
  return (
    <header className="navbar navbar-expand-md d-print-none">
      <div className="container-xl">

        <button
          className="navbar-toggler"
          type="button"
          data-bs-toggle="collapse"
          data-bs-target="#navbar-menu"
          aria-controls="navbar-menu"
          aria-expanded="false"
          aria-label="Toggle navigation"
        >
          <span className="navbar-toggler-icon"></span>
        </button>

        {/* NAVBAR LOGO */}
        <a
          href="../../.."
          aria-label="Tabler"
          className="navbar-brand navbar-brand-autodark me-3"
        >
          <svg
            xmlns="http://www.w3.org/2000/svg"
            width="110"
            height="32"
            viewBox="0 0 232 68"
            className="navbar-brand-image"
          >
            <path
              d="M64.6 16.2C63 9.9 58.1 5 51.8 3.4 40 1.5 28 1.5 16.2 3.4 9.9 5 5 9.9 3.4 16.2 1.5 28 1.5 40 3.4 51.8 5 58.1 9.9 63 16.2 64.6c11.8 1.9 23.8 1.9 35.6 0C58.1 63 63 58.1 64.6 51.8c1.9-11.8 1.9-23.8 0-35.6zM33.3 36.3c-2.8 4.4-6.6 8.2-11.1 11-1.5.9-3.3.9-4.8.1s-2.4-2.3-2.5-4c0-1.7.9-3.3 2.4-4.1 2.3-1.4 4.4-3.2 6.1-5.3-1.8-2.1-3.8-3.8-6.1-5.3-2.3-1.3-3-4.2-1.7-6.4s4.3-2.9 6.5-1.6c4.5 2.8 8.2 6.5 11.1 10.9 1 1.4 1 3.3.1 4.7zM49.2 46H37.8c-2.1 0-3.8-1-3.8-3s1.7-3 3.8-3h11.4c2.1 0 3.8 1 3.8 3s-1.7 3-3.8 3z"
              fill="#066fd1"
              style={{ fill: "var(--tblr-primary, #066fd1)" }}
            />
            <path
              d="M105.8 46.1c.4 0 .9.2 1.2.6s.6 1 .6 1.7c0 .9-.5 1.6-1.4 2.2s-2 .9-3.2.9c-2 0-3.7-.4-5-1.3s-2-2.6-2-5.4V31.6h-2.2c-.8 0-1.4-.3-1.9-.8s-.9-1.1-.9-1.9c0-.7.3-1.4.8-1.8s1.2-.7 1.9-.7h2.2v-3.1c0-.8.3-1.5.8-2.1s1.3-.8 2.1-.8 1.5.3 2 .8.8 1.3.8 2.1v3.1h3.4c.8 0 1.4.3 1.9.8s.8 1.2.8 1.9-.3 1.4-.8 1.8-1.2.7-1.9.7h-3.4v13c0 .7.2 1.2.5 1.5s.8.5 1.4.5c.3 0 .6-.1 1.1-.2.5-.2.8-.3 1.2-.3z"
              fillRule="evenodd"
              clipRule="evenodd"
              fill="#4a4a4a"
            />
          </svg>
        </a>

        <ul className="navbar-nav">
          <li className="nav-item active">
            <a className="nav-link" href="#">
              <span className="nav-link-icon">
                <svg
                  xmlns="http://www.w3.org/2000/svg"
                  width="24"
                  height="24"
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="2"
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  className="icon icon-1"
                >
                  <path d="M5 12l-2 0l9 -9l9 9l-2 0" />
                  <path d="M5 12v7a2 2 0 0 0 2 2h10a2 2 0 0 0 2 -2v-7" />
                  <path d="M9 21v-6a2 2 0 0 1 2 -2h2a2 2 0 0 1 2 2v6" />
                </svg>
              </span>
              <span className="nav-link-title">Home</span>
            </a>
          </li>

          <li className="nav-item">
            <a className="nav-link" href="/map">
              <span className="nav-link-icon">
                <svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" class="icon icon-tabler icons-tabler-outline icon-tabler-world-map"><path stroke="none" d="M0 0h24v24H0z" fill="none" /><path d="M20 8h-2a2 2 0 0 0 -2 2a2 2 0 1 1 -4 0v-1a2 2 0 0 0 -2 -2h-1a2 2 0 0 1 -2 -2v-.5" /><path d="M3 12h3a2 2 0 0 1 2 2v.5a1.5 1.5 0 0 0 1.5 1.5a1.5 1.5 0 0 1 1.5 1.5v3.25" /><path d="M15 20.5v-3.5a2 2 0 0 1 2 -2h3.5" /><path d="M3 12a9 9 0 1 0 18 0a9 9 0 1 0 -18 0" /></svg>
              </span>
              <span className="nav-link-title">Map</span>
            </a>
          </li>

          <li className="nav-item">
            <a className="nav-link" href="#">
              <span className="nav-link-icon">
                <svg
                  xmlns="http://www.w3.org/2000/svg"
                  width="24"
                  height="24"
                  viewBox="0 0 24 24"
                  fill="none"
                  stroke="currentColor"
                  strokeWidth="2"
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  className="icon icon-1"
                >
                  <path d="M9 11l3 3l8 -8" />
                  <path d="M20 12v6a2 2 0 0 1 -2 2h-12a2 2 0 0 1 -2 -2v-12a2 2 0 0 1 2 -2h9" />
                </svg>
              </span>
              <span className="nav-link-title">Settings</span>
            </a>
          </li>
        </ul>

        <div className="navbar-nav flex-row order-md-last ms-auto">
          <div className="nav-item dropdown">
            <a
              href="#"
              className="nav-link d-flex lh-1 text-reset"
              data-bs-toggle="dropdown"
              aria-label="Open user menu"
            >
              <span
                className="avatar avatar-sm"
                style={{ backgroundImage: "url(/static/avatars/044m.jpg)" }}
              ></span>

              <div className="d-none d-xl-block ps-2">
                <div>Paweł Kuna</div>
                <div className="mt-1 small text-secondary">UI Designer</div>
              </div>
            </a>

            <div className="dropdown-menu dropdown-menu-end dropdown-menu-arrow">
              <a href="#" className="dropdown-item">Status</a>
              <a href="./profile.html" className="dropdown-item">Profile</a>
              <a href="#" className="dropdown-item">Feedback</a>
              <div className="dropdown-divider"></div>
              <a href="./settings.html" className="dropdown-item">Settings</a>
              <a href="./sign-in.html" className="dropdown-item">Logout</a>
            </div>
          </div>
        </div>

      </div>
    </header>
  );
}