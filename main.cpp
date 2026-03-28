#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <iostream>
#include <vector>
#include <set>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

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

static Vector3 RotateXY(const Vector3 & Position, float Angle) {
	const float Cos = SDL_cosf(Angle);
	const float Sin = SDL_sinf(Angle);

	return {
		.X = Position.X * Cos - Position.Y * Sin,
		.Y = Position.X * Sin + Position.Y * Cos,
		.Z = Position.Z,
	};
}

static bool ClipLineNearPlane(Vector3 & A, Vector3 & B, const float NearPlane) {
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
	Vector3 Position = {0, 0, -5.0f};
	Vector3 Rotation = {0, 0, 0};

	float Speed = 5.0f;
	float SlowDownSpeed = 1.0f;
	float MouseSensitivity = 0.005f;

	void Update(const double & DeltaTime, CameraActions & Actions) {
		const float MoveDelta = (Actions.SlowDown) ? this -> SlowDownSpeed * (float) DeltaTime : this -> Speed * (float) DeltaTime;

		const Vector3 ForwardVector = {
			.X = SDL_sinf(this -> Rotation.Y) * SDL_cosf(this -> Rotation.X),
			.Y = SDL_sinf(this -> Rotation.X),
			.Z = SDL_cosf(this -> Rotation.Y) * SDL_cosf(this -> Rotation.X),
		};

		const Vector3 UpVector = {
			.X = SDL_sinf(this -> Rotation.Y) * SDL_sinf(this -> Rotation.X),
			.Y = SDL_cosf(this -> Rotation.X),
			.Z = SDL_cosf(this -> Rotation.Y) * SDL_sinf(this -> Rotation.X),
		};

		const Vector3 RightVector = {
			.X = SDL_cosf(this -> Rotation.Y),
			.Y = 0,
			.Z = -SDL_sinf(this -> Rotation.Y),
		};

		if (Actions.Forward) {
			this -> Position.X += ForwardVector.X * MoveDelta;
			this -> Position.Y += ForwardVector.Y * MoveDelta;
			this -> Position.Z += ForwardVector.Z * MoveDelta;
		}

		if (Actions.Backward) {
			this -> Position.X -= ForwardVector.X * MoveDelta;
			this -> Position.Y -= ForwardVector.Y * MoveDelta;
			this -> Position.Z -= ForwardVector.Z * MoveDelta;
		}

		if (Actions.Right) {
			this -> Position.X += RightVector.X * MoveDelta;
			this -> Position.Z += RightVector.Z * MoveDelta;
		}

		if (Actions.Left) {
			this -> Position.X -= RightVector.X * MoveDelta;
			this -> Position.Z -= RightVector.Z * MoveDelta;
		}

		if (Actions.Up) {
			this -> Position.X -= UpVector.X * MoveDelta;
			this -> Position.Y += UpVector.Y * MoveDelta;
			this -> Position.Z -= UpVector.Z * MoveDelta;
		}

		if (Actions.Down) {
			this -> Position.X += UpVector.X * MoveDelta;
			this -> Position.Y -= UpVector.Y * MoveDelta;
			this -> Position.Z += UpVector.Z * MoveDelta;
		}

		if (Actions.MouseLook) {
			this -> Rotation.X -= Actions.MouseDelta.Y * this -> MouseSensitivity;
			this -> Rotation.Y += Actions.MouseDelta.X * this -> MouseSensitivity;

			this -> Rotation.X = SDL_clamp(this -> Rotation.X, -SDL_PI_F * 0.5f, SDL_PI_F * 0.5f);

			Actions.MouseDelta.X = 0;
			Actions.MouseDelta.Y = 0;
		}

		if (Actions.Scroll != 0) {
			this -> Position.X += ForwardVector.X * this -> Speed * Actions.Scroll;
			this -> Position.Y += ForwardVector.Y * this -> Speed * Actions.Scroll;
			this -> Position.Z += ForwardVector.Z * this -> Speed * Actions.Scroll;

			Actions.Scroll = 0;
		}
	}
};

struct Mesh {
	std::vector <Vector3> Vertices;
	std::vector <std::vector <int>> Faces;

	static Mesh Cube() {
		Mesh NewMesh;

		NewMesh.Vertices = {
			{0.5f, 0.5f, 0.5f},
			{0.5f, -0.5f, 0.5f},
			{-0.5f, -0.5f, 0.5f},
			{-0.5f, 0.5f, 0.5f},

			{0.5f, 0.5f, -0.5f},
			{0.5f, -0.5f, -0.5f},
			{-0.5f, -0.5f, -0.5f},
			{-0.5f, 0.5f, -0.5f},
		};

		NewMesh.Faces = {
			{0, 1, 2 ,3},
			{4, 5, 6, 7},
			{0, 4},
			{1, 5},
			{2, 6},
			{3, 7},
		};

		return NewMesh;
	}

	static Mesh FromOBJ(const std::string & Path) {
		Mesh NewMesh;

		std::ifstream File(Path);

		if (!File.is_open()) {
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", "Failed to open .obj file!", NULL);
			return Mesh::Cube();
		}

		std::string Line;

		while (std::getline(File, Line)) {
			if (Line.empty() || Line[0] == '#') continue;

			std::stringstream Stream(Line);
			std::string Type;

			Stream >> Type;

			if (Type == "v") {
				Vector3 Vertice;

				if (Stream >> Vertice.X >> Vertice.Y >> Vertice.Z) {
					NewMesh.Vertices.push_back(Vertice);
				}
			} else if (Type == "f") {
				std::string Token;
				std::vector <int> Face;

				while (Stream >> Token) {
					try {
						size_t Slash = Token.find('/');
						int Index = std::stoi(Token.substr(0, Slash));
						Index = (Index > 0) ? (Index - 1) : static_cast <int> (NewMesh.Vertices.size() + Index);
						Face.push_back(Index);
					} catch (...) {continue;};
				}

				if (Face.size() >= 2) NewMesh.Faces.push_back(Face);
			}
		}

		return NewMesh;
	}
};

struct RenderData {
	SDL_Renderer * Renderer;
	const Camera & GlobalCamera;
	const int Width, Height;
	const float NearPlane;
};

struct Object {
	Mesh MeshData;

	Vector3 Position = {0, 0, 0};
	Vector3 Rotation = {0, 0, 0};
	Vector3 Scale = {1.0f, 1.0f, 1.0f};

	SDL_Color Color = {120, 0, 180, SDL_ALPHA_OPAQUE};

	static Vector3 TransformVertex(const Vector3 & Vertex, const Object & Target) {
		Vector3 Scaled = {
			.X = Vertex.X * Target.Scale.X,
			.Y = Vertex.Y * Target.Scale.Y,
			.Z = Vertex.Z * Target.Scale.Z,
		};

		Vector3 Rotated = RotateYZ(RotateXZ(RotateXY(Scaled,
			Target.Rotation.Z),
			Target.Rotation.Y),
			Target.Rotation.X
		);

		return {
			.X = Rotated.X + Target.Position.X,
			.Y = Rotated.Y + Target.Position.Y,
			.Z = Rotated.Z + Target.Position.Z,
		};
	}	

	virtual void Update(const double & DeltaTime) {}

	virtual void Render(const RenderData & Data) {
		for (const std::vector <int> & Face : this -> MeshData.Faces) {
			const int N = (int) Face.size();

			for (int VertexIndex = 0; VertexIndex < N; ++VertexIndex) {
				const Vector3 A = Object::TransformVertex(this -> MeshData.Vertices[Face[VertexIndex]], *this);
				const Vector3 B = Object::TransformVertex(this -> MeshData.Vertices[Face[(VertexIndex + 1) % N]], *this);

				const Vector3 ShiftedA = {
					.X = A.X - Data.GlobalCamera.Position.X,
					.Y = A.Y - Data.GlobalCamera.Position.Y,
					.Z = A.Z - Data.GlobalCamera.Position.Z,
				};

				const Vector3 ShiftedB = {
					.X = B.X - Data.GlobalCamera.Position.X,
					.Y = B.Y - Data.GlobalCamera.Position.Y,
					.Z = B.Z - Data.GlobalCamera.Position.Z,
				};

				Vector3 RotatedA = RotateYZ(RotateXZ(RotateXY(ShiftedA,
					Data.GlobalCamera.Rotation.Z),
					Data.GlobalCamera.Rotation.Y),
					Data.GlobalCamera.Rotation.X
				);

				Vector3 RotatedB = RotateYZ(RotateXZ(RotateXY(ShiftedB,
					Data.GlobalCamera.Rotation.Z),
					Data.GlobalCamera.Rotation.Y),
					Data.GlobalCamera.Rotation.X
				);

				if (!ClipLineNearPlane(RotatedA, RotatedB, Data.NearPlane)) continue;

				Vector2 ProjectedA = ProjectFrom3D(RotatedA);
				Vector2 ProjectedB = ProjectFrom3D(RotatedB);

				Vector2 WindowedB = ToWindowSpace(ProjectedB, Data.Width, Data.Height);
				Vector2 WindowedA = ToWindowSpace(ProjectedA, Data.Width, Data.Height);

				SetDrawColor(Data.Renderer, this -> Color);
				SDL_RenderLine(Data.Renderer, WindowedA.X, WindowedA.Y, WindowedB.X, WindowedB.Y);
			}
		}
	}
};

struct SpinningCube : public Object {
	float RotationSpeed = 1.0f;

	SpinningCube() {
		this -> MeshData = Mesh::Cube();
	}

	void Update(const double & DeltaTime) override {
		this -> Rotation.X += RotationSpeed * 0.5f * (float) DeltaTime;
		this -> Rotation.Y += RotationSpeed * 1.0f * (float) DeltaTime;
		this -> Rotation.Z += RotationSpeed * 0.2f * (float) DeltaTime;
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

	std::vector <Object*> Scene;
};

static void InitializeScene(std::vector <Object*> & Scene) {
	Object * RotatedCube = new Object();
	RotatedCube-> MeshData = Mesh::Cube();
	RotatedCube-> Position = {-2.0f, 0, 0};
	RotatedCube-> Rotation = {20.0f, 20.0f, 20.0f};
	Scene.emplace_back(RotatedCube);

	Object * ScaledCube = new Object();
	ScaledCube -> MeshData = Mesh::Cube();
	ScaledCube -> Position = {2.0f, 0, 0};
	ScaledCube -> Scale = {1.5f, 2.0f, 0.5f};
	ScaledCube -> Color = {0, 255, 0, SDL_ALPHA_OPAQUE};
	Scene.emplace_back(ScaledCube);

	SpinningCube * BehaviourCube = new SpinningCube();
	BehaviourCube -> Color = {255, 0, 0, SDL_ALPHA_OPAQUE};
	Scene.push_back(BehaviourCube);

	Object * Teapot = new Object();
	Teapot -> MeshData = Mesh::FromOBJ("teapot.obj");
	Teapot -> Position = {0, -2.5f, 10.0f};
	Teapot -> Scale = {0.1f, 0.1f, 0.1f};
	Teapot -> Color = {255, 255, 0, SDL_ALPHA_OPAQUE};
	Scene.emplace_back(Teapot);

	/*

	Object * Bunny = new Object();
	Bunny -> MeshData = Mesh::FromOBJ("stanford-bunny.obj");
	Bunny -> Position = {0, 0, 10};
	Bunny -> Scale = {50.0f, 50.0f, 50.0f};
	Bunny -> Color = {255, 255, 0, SDL_ALPHA_OPAQUE};
	Scene.emplace_back(Bunny);

	*/
}

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

	InitializeScene(State -> Scene);

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
		State -> GlobalCameraActions.MouseDelta.X += Event -> motion.xrel;
		State -> GlobalCameraActions.MouseDelta.Y += Event -> motion.yrel;
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

	RenderData Data = {
		.Renderer = State -> Renderer,
		.GlobalCamera = State -> GlobalCamera,
		.Width = State -> WindowWidth,
		.Height = State -> WindowHeight,
		.NearPlane = State -> NearClipPlane,
	};

	for (Object * Target : State -> Scene) {
		Target -> Update(DeltaTime);
		Target -> Render(Data);
	}

	SDL_RenderPresent(State -> Renderer);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void * App, SDL_AppResult Result) {
	AppState * State = (AppState*) App;

	if (State) {
		if (State -> Renderer) SDL_DestroyRenderer(State -> Renderer);
		if (State -> Window) SDL_DestroyWindow(State -> Window);

		for (Object * Target : State -> Scene) {
			delete Target;
		}

		delete State;
	}

	SDL_Quit();
}