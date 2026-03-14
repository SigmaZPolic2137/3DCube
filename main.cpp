#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <vector>

struct Vector2 {
	float X, Y;
};

struct Vector3 {
	float X, Y, Z;
};

static bool ToggleFullscreen(SDL_Window * Window) {
	SDL_WindowFlags Flags = SDL_GetWindowFlags(Window);
	bool IsFullscreen = Flags & SDL_WINDOW_FULLSCREEN;

	if (!IsFullscreen) {
		SDL_SetWindowFullscreen(Window, true);
	} else {
		SDL_RestoreWindow(Window);
		SDL_SetWindowFullscreen(Window, false);
	}

	return !IsFullscreen;
}

static void SetDrawColor(SDL_Renderer * Renderer, const SDL_Color & Color) {
	SDL_SetRenderDrawColor(Renderer, Color.r, Color.g, Color.b, Color.a);
}

static Vector2 ToWindowSpace(const Vector2 & Point, int Width, int Height) {
	float Scale = (float) SDL_min(Width, Height);

	float OffsetX = (Width - Scale) * 0.5f;
	float OffsetY = (Height - Scale) * 0.5f;

	return {
		.X = OffsetX + (Point.X + 1.0f) * 0.5f * Scale,
		.Y = OffsetY + (1.0f - Point.Y) * 0.5f * Scale,
	};
}

static Vector2 ProjectFrom3D(const Vector3 & Position) {
	return {
		.X = Position.X / Position.Z,
		.Y = Position.Y / Position.Z,
	};
}

static Vector3 RotateYZ(const Vector3 & Position, float Angle) {
	const float Cos = SDL_cosf(Angle);
	const float Sin = SDL_sinf(Angle);

	return {
		.X = Position.X,
		.Y = Position.Y * Cos - Position.Z * Sin,
		.Z = Position.Y * Sin + Position.Z * Cos,
	};
}

static Vector3 RotateXZ(const Vector3 & Position, float Angle) {
	const float Cos = SDL_cosf(Angle);
	const float Sin = SDL_sinf(Angle);

	return {
		.X = Position.X * Cos - Position.Z * Sin,
		.Y = Position.Y,
		.Z = Position.X * Sin + Position.Z * Cos,
	};
}

static Vector3 RotateXY(const Vector3& Position, float Angle) {
	const float Cos = SDL_cosf(Angle);
	const float Sin = SDL_sinf(Angle);

	return {
		.X = Position.X * Cos - Position.Y * Sin,
		.Y = Position.X * Sin + Position.Y * Cos,
		.Z = Position.Z,
	};
}

static bool ClipLineNearPlane(Vector3 & A, Vector3 & B, float NearPlane) {
	if (A.Z >= NearPlane && B.Z >= NearPlane) return true;
	if (A.Z < NearPlane && B.Z < NearPlane) return false;

	float Alpha = (NearPlane - A.Z) / (B.Z - A.Z);

	Vector3 Intersection = {
		A.X + Alpha * (B.X - A.X),
		A.Y + Alpha * (B.Y - A.Y),
		NearPlane
	};

	if (A.Z < NearPlane) A = Intersection;
	else B = Intersection;

	return true;
}


struct CameraActions {
	bool Forward = false;
	bool Backward = false;

	bool Right = false;
	bool Left = false;

	bool Up = false;
	bool Down = false;

	bool MouseLook = false;
	bool SlowDown = false;

	Vector2 MouseDelta = {0.0f, 0.0f};

	float Scroll = 0.0f;
};

struct Camera {
	Vector3 Position = {0, 0, -1.0f};
	Vector3 Rotation = {0, 0, 0};

	float Speed = 5.0f;
	float SlowDownSpeed = 1.0f;
	float MouseSensitivity = 1.0f;

	void Update(const double & DeltaTime, CameraActions & Actions) {
		const float MoveDelta = (Actions.SlowDown) ? this -> SlowDownSpeed * (float) DeltaTime : this -> Speed * (float) DeltaTime;

		const Vector3 LookVector = {
			SDL_sinf(this -> Rotation.Y) * SDL_cosf(this -> Rotation.X),
			SDL_sinf(this -> Rotation.X),
			SDL_cosf(this -> Rotation.Y) * SDL_cosf(this -> Rotation.X),
		};

		const Vector3 RightVector = {
			SDL_cosf(this -> Rotation.Y),
			0,
			-SDL_sinf(this -> Rotation.Y)
		};

		if (Actions.Forward) {
			this -> Position.X += LookVector.X * MoveDelta;
			this -> Position.Y += LookVector.Y * MoveDelta;
			this -> Position.Z += LookVector.Z * MoveDelta;
		}

		if (Actions.Backward) {
			this -> Position.X -= LookVector.X * MoveDelta;
			this -> Position.Y -= LookVector.Y * MoveDelta;
			this -> Position.Z -= LookVector.Z * MoveDelta;
		}

		if (Actions.Right) {
			this -> Position.X += RightVector.X * MoveDelta;
			this -> Position.Z += RightVector.Z * MoveDelta;
		}

		if (Actions.Left) {
			this -> Position.X -= RightVector.X * MoveDelta;
			this -> Position.Z -= RightVector.Z * MoveDelta;
		}

		if (Actions.Up) this -> Position.Y += MoveDelta;
		if (Actions.Down) this -> Position.Y -= MoveDelta;

		if (Actions.MouseLook) {
			this -> Rotation.X -= Actions.MouseDelta.Y * (float) DeltaTime * this -> MouseSensitivity;
			this -> Rotation.Y += Actions.MouseDelta.X * (float) DeltaTime * this -> MouseSensitivity;

			Actions.MouseDelta.X = 0;
			Actions.MouseDelta.Y = 0;
		}

		if (Actions.Scroll != 0) {
			this -> Position.X += LookVector.X * this -> Speed * Actions.Scroll;
			this -> Position.Y += LookVector.Y * this -> Speed * Actions.Scroll;
			this -> Position.Z += LookVector.Z * this -> Speed * Actions.Scroll;

			Actions.Scroll = 0;
		}
	}
};

struct AppState {
	const char * AppName = "3D Cube";

	SDL_Window * Window = nullptr;
	SDL_Renderer * Renderer = nullptr;

	int WindowWidth = 800;
	int WindowHeight = 800;

	bool VSync = true;
	bool Fullscreen = false;

	Uint64 PreviousCounter = SDL_GetPerformanceCounter();

	Camera GlobalCamera = Camera();
	CameraActions GlobalCameraActions = CameraActions();

	SDL_Color BackgroundColor = {0, 0, 0, SDL_ALPHA_OPAQUE};
	SDL_Color ForegroundColor = {120, 0, 180, SDL_ALPHA_OPAQUE};

	float NearClipPlane = 0.05f;

	std::vector <Vector3> Vertices = {
		{0.25f, 0.25f, 0.25f},
		{0.25f, -0.25f, 0.25f},
		{-0.25f, -0.25f, 0.25f},
		{-0.25f, 0.25f, 0.25f},

		{0.25f, 0.25f, -0.25f},
		{0.25f, -0.25f, -0.25f},
		{-0.25f, -0.25f, -0.25f},
		{-0.25f, 0.25f, -0.25f},
	};

	std::vector <std::vector <int>> Faces = {
		{0, 1, 2 ,3},
		{4, 5, 6, 7},
		{0, 4},
		{1, 5},
		{2, 6},
		{3, 7},
	};
};

SDL_AppResult SDL_AppInit(void ** App, int ArgumentCount, char * ArgumentValues[]) {
	if (!SDL_Init(SDL_INIT_VIDEO)) return SDL_APP_FAILURE;

	AppState * State = new AppState();
	*App = State;

	if (!SDL_CreateWindowAndRenderer(State -> AppName,
		State -> WindowWidth, State -> WindowHeight,
		SDL_WINDOW_RESIZABLE, &State -> Window, &State -> Renderer)) {
		return SDL_APP_FAILURE;
	}

	SDL_SetRenderDrawBlendMode(State -> Renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderVSync(State -> Renderer, State -> VSync);
	SDL_SetWindowFullscreen(State -> Window, State -> Fullscreen);

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void * App, SDL_Event * Event) {
	AppState * State = (AppState*) App;

	if (Event -> type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

	if (Event -> type == SDL_EVENT_WINDOW_RESIZED) SDL_GetWindowSize(State -> Window, &State -> WindowWidth, &State -> WindowHeight);

	if (Event -> type == SDL_EVENT_KEY_DOWN) {
		SDL_Keycode Keycode = Event -> key.key;

		if (Keycode == SDLK_ESCAPE) return SDL_APP_SUCCESS;

		if (Keycode == SDLK_F11) {
			State -> Fullscreen = ToggleFullscreen(State -> Window);
		}

		if (Keycode == SDLK_W) State -> GlobalCameraActions.Forward = true;
		if (Keycode == SDLK_S) State -> GlobalCameraActions.Backward = true;

		if (Keycode == SDLK_D) State -> GlobalCameraActions.Right = true;
		if (Keycode == SDLK_A) State -> GlobalCameraActions.Left = true;

		if (Keycode == SDLK_E) State -> GlobalCameraActions.Up = true;
		if (Keycode == SDLK_Q) State -> GlobalCameraActions.Down = true;

		if (Keycode == SDLK_LSHIFT) State -> GlobalCameraActions.SlowDown = true;
	}

	if (Event -> type == SDL_EVENT_KEY_UP) {
		SDL_Keycode Keycode = Event -> key.key;

		if (Keycode == SDLK_W) State -> GlobalCameraActions.Forward = false;
		if (Keycode == SDLK_S) State -> GlobalCameraActions.Backward = false;

		if (Keycode == SDLK_D) State -> GlobalCameraActions.Right = false;
		if (Keycode == SDLK_A) State -> GlobalCameraActions.Left = false;

		if (Keycode == SDLK_E) State -> GlobalCameraActions.Up = false;
		if (Keycode == SDLK_Q) State -> GlobalCameraActions.Down = false;

		if (Keycode == SDLK_LSHIFT) State -> GlobalCameraActions.SlowDown = false;
	}

	if (Event -> type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
		Uint8 Button = Event -> button.button;

		if (Button == SDL_BUTTON_RIGHT) {
			State -> GlobalCameraActions.MouseLook = true;
			SDL_SetWindowRelativeMouseMode(State -> Window, true);
		}
	}

	if (Event -> type == SDL_EVENT_MOUSE_BUTTON_UP) {
		Uint8 Button = Event -> button.button;

		if (Button == SDL_BUTTON_RIGHT) {
			State -> GlobalCameraActions.MouseLook = false;
			SDL_SetWindowRelativeMouseMode(State -> Window, false);
		}
	}

	if (Event -> type == SDL_EVENT_MOUSE_MOTION && State -> GlobalCameraActions.MouseLook) {
		State -> GlobalCameraActions.MouseDelta = {Event -> motion.xrel, Event -> motion.yrel};
	}

	if (Event -> type == SDL_EVENT_MOUSE_WHEEL) {
		State -> GlobalCameraActions.Scroll = Event -> wheel.y;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void * App) {
	AppState * State = (AppState*) App;

	Uint64 CurrentCounter = SDL_GetPerformanceCounter();
	double DeltaTime = (double) (CurrentCounter - State -> PreviousCounter) / SDL_GetPerformanceFrequency();
	State -> PreviousCounter = CurrentCounter;

	char Title[128];
	snprintf(Title, sizeof(Title), "%s - %d FPS", State -> AppName, static_cast <int> (1 / DeltaTime));
	SDL_SetWindowTitle(State -> Window, Title);

	State -> GlobalCamera.Update(DeltaTime, State -> GlobalCameraActions);

	SetDrawColor(State -> Renderer, State -> BackgroundColor);
	SDL_RenderClear(State -> Renderer);
	SetDrawColor(State -> Renderer, State -> ForegroundColor);

	for (const std::vector <int> & Face : State -> Faces) {
		const int N = (int) Face.size();
		 
		for (int VerticeIndex = 0; VerticeIndex < N; ++VerticeIndex) {
			const Vector3 A = State -> Vertices[Face[VerticeIndex]];
			const Vector3 B = State -> Vertices[Face[(VerticeIndex + 1) % N]];

			Vector3 ShiftedA = {
				A.X - State -> GlobalCamera.Position.X,
				A.Y - State -> GlobalCamera.Position.Y,
				A.Z - State -> GlobalCamera.Position.Z,
			};

			Vector3 ShiftedB = {
				B.X - State -> GlobalCamera.Position.X,
				B.Y - State -> GlobalCamera.Position.Y,
				B.Z - State -> GlobalCamera.Position.Z,
			};

			Vector3 RotatedA = RotateYZ(RotateXZ(RotateXY(ShiftedA,
				State -> GlobalCamera.Rotation.Z),
				State -> GlobalCamera.Rotation.Y),
				State -> GlobalCamera.Rotation.X
			);

			Vector3 RotatedB = RotateYZ(RotateXZ(RotateXY(ShiftedB,
				State -> GlobalCamera.Rotation.Z),
				State -> GlobalCamera.Rotation.Y),
				State -> GlobalCamera.Rotation.X
			);

			if (!ClipLineNearPlane(RotatedA, RotatedB, State -> NearClipPlane)) continue;

			Vector2 ProjectedA = ProjectFrom3D(RotatedA);
			Vector2 ProjectedB = ProjectFrom3D(RotatedB);

			Vector2 WindowedB = ToWindowSpace(ProjectedB, State -> WindowWidth, State -> WindowHeight);
			Vector2 WindowedA = ToWindowSpace(ProjectedA, State -> WindowWidth, State -> WindowHeight);

			SDL_RenderLine(State -> Renderer, WindowedA.X, WindowedA.Y, WindowedB.X, WindowedB.Y);
		}
	}

	SDL_RenderPresent(State -> Renderer);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void * App, SDL_AppResult Result) {
	AppState * State = (AppState*) App;

	if (State) {
		if (State -> Renderer) SDL_DestroyRenderer(State -> Renderer);
		if (State -> Window) SDL_DestroyWindow(State -> Window);
		delete State;
	}

	SDL_Quit();
}