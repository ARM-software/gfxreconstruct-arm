---
name: gfxreconstruct-code-style
description: Match nearby GFXReconstruct code when local style choices are unclear.
---

# GFXReconstruct code style

- Match the naming and structure of the closest implementation of the same kind, for example: Vulkan hooks often keep API parameter names, class data members typically have trailing underscores.
- Use `GFXRECON_BEGIN_NAMESPACE` and `GFXRECON_END_NAMESPACE` for project namespaces.
- Reuse existing helpers. Extract a single-use helper only if it makes the caller easier to follow.
- If a condition’s purpose is hard to see, name it after what it decides, such as `can_reuse_allocation`.
- Prefer private member functions for class-specific helpers. Avoid lambdas for those helpers. Do not use anonymous namespaces unless absolutely necessary.
- In Vulkan tests, use named flags where helpful and explain command order when it affects the result.
- Review the diff against nearby code and the [repository coding guidelines](../../../CONTRIBUTING.md#coding-guidelines). Keep style edits scoped to the change.
