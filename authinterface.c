#include <windows.h>

#define ID_USERNAME 1
#define ID_EMAIL 2
#define ID_PASSWORD 3
#define ID_SUBMIT 4

HWND hUsername, hEmail, hPassword;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // Label et champ Pseudo
            CreateWindow("STATIC", "Pseudo :", WS_VISIBLE | WS_CHILD, 20, 20, 80, 25, hwnd, NULL, NULL, NULL);
            hUsername = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 20, 200, 25, hwnd, (HMENU)ID_USERNAME, NULL, NULL);

            // Label et champ Email
            CreateWindow("STATIC", "Email :", WS_VISIBLE | WS_CHILD, 20, 60, 80, 25, hwnd, NULL, NULL, NULL);
            hEmail = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER, 100, 60, 200, 25, hwnd, (HMENU)ID_EMAIL, NULL, NULL);

            // Label et champ Mot de passe
            CreateWindow("STATIC", "Mot de passe :", WS_VISIBLE | WS_CHILD, 20, 100, 100, 25, hwnd, NULL, NULL, NULL);
            hPassword = CreateWindow("EDIT", "", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_PASSWORD, 120, 100, 180, 25, hwnd, (HMENU)ID_PASSWORD, NULL, NULL);

            // Bouton
            CreateWindow("BUTTON", "Créer le compte", WS_VISIBLE | WS_CHILD, 100, 150, 150, 30, hwnd, (HMENU)ID_SUBMIT, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_SUBMIT) {
                char username[100], email[100], password[100];
                GetWindowText(hUsername, username, sizeof(username));
                GetWindowText(hEmail, email, sizeof(email));
                GetWindowText(hPassword, password, sizeof(password));

                MessageBox(hwnd, "Formulaire soumis ! (DB à venir)", "Info", MB_OK);
                // Tu pourras ajouter ici l’appel PostgreSQL
            }
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    const char CLASS_NAME[] = "FormWindowClass";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, CLASS_NAME, "Créer un compte", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 400, 250,
                               NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {0};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

// cricr@KERA MINGW64 /c/Users/cricr/Documents/Professionnel/LaPlateforme_/C/Discord/MyDiscord
// $ gcc authinterface.c -o authinterface.exe -I"C:\PostgreSQL\include" -L"C:\PostgreSQL\lib" -lpq -lws2_32 -mwindows
