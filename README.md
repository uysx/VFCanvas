# VFCanvas
VFCanvas is a free open-source alternative to IDO's Watch Face Designer "cn-iwf.idoocloud.com" for creating watch faces for smartwatches that use the VeryFit app.

---

## Why Choose VFCanvas
VFCanvas replaces  IDO's watch face designer (Which required an IDO account) and old, buggy open-source tools (Such as CadranEditor) which required complex SDK or kit installations. Instead of a GUI application, VFCanvas will be a browser app that requires zero installations (*Note: VFCanvas doesn't have watch face compiling; Instead, watch face compilation is be separate and written in C.*).

Differences:
| VFCanvas                  | IDO Watch Face Designer
| -------------             | -------------
| Open-Source               | Closed-Source
| No account required       | Requires IDO Account
| Has Watch Face Compiler (.iwf): | No .iwf compiler; Relies on confidential SDKs
| GPL-3.0 License           | No License; Tool is intellectual property
| No registration or login  | Can register IDO account, but has to agree to the strict user agreement
| Built-In Device Catalog   | No built-in device catalog; Each account has their own devices or none
| Modifications Allowed     | Modifications Prohibited
| Distributions Allowed     | Distributions Prohibited

## Watch Face Compiling
To compile watch faces, you will use the pre-compiled executable **compile_dial.exe**. The compiler also has source code available if anyone wants to improve the code or add more features to the code.

### Usage:
`.\compile_dial.exe input_folder output_iwf`

---

## Screenshots (Tool is in development)
<img width="2560" height="1307" alt="Screenshot 2026-11-23 at 16-03-25 VFCanvas" src="https://github.com/user-attachments/assets/594f0740-2869-4c4c-8756-d714eddd2ca6" />
<img width="2560" height="1307" alt="Screenshot 2026-11-23 at 16-03-36 VFCanvas" src="https://github.com/user-attachments/assets/d55d65c4-7ac4-4e85-bfdd-d31930586693" />

## License
GPL-3.0
