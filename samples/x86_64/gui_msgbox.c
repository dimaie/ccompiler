/* =========================================================================
 * Win32 GUI MessageBox Sample for x86_64
 * Compiles and links against user32.dll using embedded PE linker.
 * ========================================================================= */

int MessageBoxA(void *hwnd, const char *text, const char *caption, int type);

int main() {
    MessageBoxA(0, "Hello from native Windows x86-64 GUI binary!", "CCompiler", 0);
    return 0;
}
