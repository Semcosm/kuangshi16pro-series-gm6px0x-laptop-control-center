# CI And Branch Protection

GitHub Actions now defines one workflow, `ci`, with these job names:

- `lint`
- `build`
- `test`
- `integration-smoke`

Recommended required checks for the `main` protected branch:

- `ci / lint`
- `ci / build`
- `ci / test`
- `ci / integration-smoke`

What each check covers:

- `ci / lint`
  project-script shell syntax, optional `shellcheck`, and trailing-whitespace hygiene
- `ci / build`
  clean build of `lccctl` and `lccd`
- `ci / test`
  unit-test binary build and execution
- `ci / integration-smoke`
  mock-backed `lccd` startup, D-Bus introspection, and product-command happy path

Suggested branch protection settings:

1. Enable branch protection for `main`.
2. Require status checks to pass before merging.
3. Add the four checks listed above.
4. Require branches to be up to date before merging.
5. Optionally require pull requests and disallow direct pushes.
