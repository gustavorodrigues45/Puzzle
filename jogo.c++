/*
 * Trabalho Prático G1 - Jogo Pedagógico: Tangram Infinito
 * Disciplina: Computação Gráfica
 * Autores: Fernando Munir Velho Schmitt e Gustavo da Encarnação Rodrigues
 *
 * Descrição: Jogo educativo baseado em Tangram onde o usuário posiciona,
 * rotaciona e redimensiona peças geométricas para encaixá-las em um padrão alvo.
 *
 * Requisitos implementados:
 * - 5 objetos geométricos (2 triângulos grandes, 1 médio, 1 quadrado, 1 paralelogramo)
 * - Transformações: translação (mouse), rotação (tecla R), escala (teclas +/-)
 * - Interação: mouse para arrastar peças, teclado para manipulação
 * - Progressão de fases com dificuldade crescente
 */

#include <GL/freeglut.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <array>
#include <sstream>
#include <string>
#include <vector>

// Representa um ponto 2D no espaço SRU (Sistema de Referência do Universo)
struct Vec2 {
	float x;
	float y;
	Vec2() = default;
	Vec2(float x, float y) : x(x), y(y) {}
};

float randomRange(float minValue, float maxValue) {
	float t = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
	return minValue + t * (maxValue - minValue);
}

// SRO (Sistema de Referência do Objeto): Cada peça armazena sua geometria em coordenadas locais
// e aplica transformações (posição, rotação, escala) para posicionamento no SRU
struct Piece {
	std::vector<Vec2> baseShape;  // Vértices da peça em coordenadas locais (SRO)
	Vec2 position{0.0f, 0.0f};   // Translação no SRU
	float rotation = 0.0f;        // Rotação em graus
	float scale = 1.0f;           // Fator de escala uniforme
	float color[3] = {1.0f, 1.0f, 1.0f};
	Vec2 targetPosition{0.0f, 0.0f};   // Posição alvo para alinhamento automático
	float targetRotation = 0.0f;       // Rotação alvo
	float targetScale = 1.0f;          // Escala alvo
	bool snapped = false;              // Indica se peça está no alvo

	static float degToRad(float deg) {
		constexpr float pi = 3.14159265359f;
		return deg * pi / 180.0f;
	}

	static float normalizeAngle(float angle) {
		angle = std::fmod(angle, 360.0f);
		return angle < 0.0f ? angle + 360.0f : angle;
	}

	static float angleDistance(float a, float b) {
		a = normalizeAngle(a);
		b = normalizeAngle(b);
		float diff = std::fabs(a - b);
		return std::min(diff, 360.0f - diff);
	}

	// Transforma vértices de SRO para SRU: aplicar escala, rotação e translação
	// Ordem: escala local → rotação → translação para posição final
	std::vector<Vec2> transformedShape(const Vec2& origin, float rot, float scl) const {
		std::vector<Vec2> result;
		result.reserve(baseShape.size());
		float r = degToRad(rot);
		float cs = std::cos(r);
		float sn = std::sin(r);

		for (const auto& local : baseShape) {
			float sx = local.x * scl;
			float sy = local.y * scl;
			result.push_back({origin.x + sx * cs - sy * sn, origin.y + sx * sn + sy * cs});
		}

		return result;
	}

	std::vector<Vec2> worldShape() const {
		return transformedShape(position, rotation, scale);
	}

	std::vector<Vec2> targetShape() const {
		return transformedShape(targetPosition, targetRotation, targetScale);
	}

	// Detecção de ponto dentro de polígono: algoritmo de raycasting
	// Verifica se o ponto está contido na peça atual para seleção com mouse
	bool containsPoint(const Vec2& point) const {
		std::vector<Vec2> poly = worldShape();
		if (poly.size() < 3) return false;

		bool inside = false;
		for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
			const Vec2& vi = poly[i];
			const Vec2& vj = poly[j];
			bool intersect = ((vi.y > point.y) != (vj.y > point.y)) &&
				(point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y + 0.00001f) + vi.x);
			if (intersect) {
				inside = !inside;
			}
		}
		return inside;
	}

	void snapToTarget() {
		position = targetPosition;
		rotation = targetRotation;
		scale = targetScale;
		snapped = true;
	}

	void setRandomStart(float xMin, float xMax, float yMin, float yMax, float maxAllowedScale) {
		position = Vec2{randomRange(xMin, xMax), randomRange(yMin, yMax)};
		rotation = randomRange(0.0f, 359.0f);
		scale = randomRange(0.78f, 0.98f) * maxAllowedScale;
		snapped = false;
	}

	void setRandomTarget(float xMin, float xMax, float yMin, float yMax, float scaleMultiplier) {
		targetPosition = Vec2{randomRange(xMin, xMax), randomRange(yMin, yMax)};
		targetRotation = randomRange(0.0f, 359.0f);
		targetScale = std::max(0.5f, std::min(1.7f, randomRange(0.8f, 1.2f) * scaleMultiplier));
	}
};

struct PiecePose {
	Vec2 position;
	float rotation;
	float scale;
};

struct TargetTemplate {
	float frameColor[3];
	std::array<PiecePose, 5> poses;
};

// Gerenciador principal do jogo: lógica de renderização, interação e física
// SRU: coordenadas do mundo vão de [-10, 10] em X e [-5.625, 5.625] em Y (proporção 16:9)
class PuzzleGame {
public:
	static constexpr int windowWidth = 1280;
	static constexpr int windowHeight = 720;
	// Tolerâncias para alinhamento automático de peças (snap)
	static constexpr float snapPosDist = 0.6f;     // Distância máxima em posição
	static constexpr float snapRotDist = 10.0f;    // Diferença máxima em rotação (graus)
	static constexpr float snapScaleDist = 0.12f;  // Diferença máxima em escala
	static constexpr float minScale = 0.5f;
	static constexpr float maxScale = 1.7f;
	static constexpr float scaleStep = 0.08f;
	static constexpr float extraTimeBonus = 15.0f;

	// UI Colors
	static constexpr float bgColorR = 0.95f, bgColorG = 0.96f, bgColorB = 0.98f;
	static constexpr float borderColorR = 0.83f, borderColorG = 0.85f, borderColorB = 0.9f;
	static constexpr float textColorR = 0.2f, textColorG = 0.25f, textColorB = 0.35f;

	PuzzleGame() = default;

	void initialize() {
		std::srand(static_cast<unsigned int>(std::time(nullptr)));
		lastTickMs = glutGet(GLUT_ELAPSED_TIME);
		buildPieces();
		resetLevel(1);
	}

	void render() {
		glClearColor(bgColorR, bgColorG, bgColorB, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		setupProjection();
		drawBoard();
		drawTargets();
		drawPieces();
		drawHud();
		drawOverlay();

		glutSwapBuffers();
	}

	void onKeyboard(unsigned char key) {
		if (key == 'n' || key == 'N') {
			resetLevel(1);
			return;
		}

		if (gameOver || selectedPiece < 0 || selectedPiece >= static_cast<int>(pieces.size())) {
			return;
		}

		Piece& piece = pieces[selectedPiece];
		if (piece.snapped) {
			return;
		}

		if (key == 'r' || key == 'R') {
			piece.rotation = Piece::normalizeAngle(piece.rotation + 15.0f);
		} else if (key == '+' || key == '=') {
			piece.scale = std::min(piece.targetScale, piece.scale + scaleStep);
		} else if (key == '-' || key == '_') {
			piece.scale = std::max(minScale, piece.scale - scaleStep);
		}

		trySnapPiece(piece);
		checkLevelCompletion();
	}

	void onMouse(int button, int state, int x, int y) {
		if (gameOver) {
			return;
		}

		Vec2 mouseWorld = screenToWorld(x, y);

		if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
			if (isInsideTimeButton(mouseWorld) && !extraTimeUsed) {
				timeRemaining += extraTimeBonus;
				extraTimeUsed = true;
				glutPostRedisplay();
				return;
			}

			selectedPiece = -1;
			for (int i = static_cast<int>(pieces.size()) - 1; i >= 0; --i) {
				if (!pieces[i].snapped && pieces[i].containsPoint(mouseWorld)) {
					selectedPiece = i;
					isDragging = true;
					dragOffset = {pieces[i].position.x - mouseWorld.x, pieces[i].position.y - mouseWorld.y};
					if (i != static_cast<int>(pieces.size()) - 1) {
						Piece top = pieces[i];
						pieces.erase(pieces.begin() + i);
						pieces.push_back(top);
						selectedPiece = static_cast<int>(pieces.size()) - 1;
					}
					break;
				}
			}
		}

		if (button == GLUT_LEFT_BUTTON && state == GLUT_UP) {
			if (selectedPiece >= 0) {
				trySnapPiece(pieces[selectedPiece]);
				checkLevelCompletion();
			}
			isDragging = false;
		}
	}

	void onMotion(int x, int y) {
		if (gameOver || !isDragging || selectedPiece < 0 || selectedPiece >= static_cast<int>(pieces.size())) {
			return;
		}

		Vec2 mouseWorld = screenToWorld(x, y);
		Piece& piece = pieces[selectedPiece];
		piece.position.x = mouseWorld.x + dragOffset.x;
		piece.position.y = mouseWorld.y + dragOffset.y;

		trySnapPiece(piece);
		checkLevelCompletion();
		glutPostRedisplay();
	}

	void onTimer() {
		int now = glutGet(GLUT_ELAPSED_TIME);
		int dtMs = now - lastTickMs;
		lastTickMs = now;

		if (!gameOver) {
			timeRemaining -= static_cast<float>(dtMs) / 1000.0f;
			if (timeRemaining <= 0.0f) {
				timeRemaining = 0.0f;
				gameOver = true;
				selectedPiece = -1;
				isDragging = false;
			}
		}

		if (gameWonFlash) {
			flashTimer += static_cast<float>(dtMs) / 1000.0f;
			if (flashTimer >= 0.9f) {
				gameWonFlash = false;
				flashTimer = 0.0f;
			}
		}

		glutPostRedisplay();
		glutTimerFunc(16, &PuzzleGame::timerThunk, 0);
	}

	static PuzzleGame& instance() {
		static PuzzleGame game;
		return game;
	}

	static void displayThunk() { instance().render(); }
	static void keyboardThunk(unsigned char key, int, int) { instance().onKeyboard(key); }
	static void mouseThunk(int button, int state, int x, int y) { instance().onMouse(button, state, x, y); }
	static void motionThunk(int x, int y) { instance().onMotion(x, y); }
	static void timerThunk(int) { instance().onTimer(); }

private:
	std::vector<Piece> pieces;
	std::vector<TargetTemplate> templates;
	int selectedPiece = -1;
	bool isDragging = false;
	Vec2 dragOffset{0.0f, 0.0f};
	int currentLevel = 1;
	float timeRemaining = 60.0f;
	bool gameOver = false;
	bool gameWonFlash = false;
	bool extraTimeUsed = false;
	float flashTimer = 0.0f;
	int lastTickMs = 0;

	static constexpr float timeButtonLeft = -9.55f;
	static constexpr float timeButtonRight = -6.75f;
	static constexpr float timeButtonBottom = -4.45f;
	static constexpr float timeButtonTop = -3.65f;

	static Vec2 screenToWorld(int x, int y) {
		float worldX = (static_cast<float>(x) / static_cast<float>(windowWidth)) * 20.0f - 10.0f;
		float worldY = ((static_cast<float>(windowHeight - y) / static_cast<float>(windowHeight)) * 11.25f) - 5.625f;
		return {worldX, worldY};
	}

	void setupProjection() const {
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(-10.0, 10.0, -5.625, 5.625, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
	}

	void buildPieces() {
		pieces.clear();
		templates.clear();

		// SRO das 5 peças do Tangram:
		// Cada peça é definida por seus vértices em coordenadas locais,
		// depois transformadas para SRU com posição, rotação e escala

		auto createPiece = [](std::vector<Vec2>&& shape, float r, float g, float b) {
			Piece p;
			p.baseShape = std::move(shape);
			p.color[0] = r; p.color[1] = g; p.color[2] = b;
			return p;
		};

		pieces.push_back(createPiece({{0.0f, 0.0f}, {1.9f, 0.0f}, {0.0f, 1.9f}}, 0.93f, 0.33f, 0.26f));
		pieces.push_back(createPiece({{0.0f, 0.0f}, {-1.9f, 0.0f}, {0.0f, 1.9f}}, 0.99f, 0.74f, 0.18f));
		pieces.push_back(createPiece({{0.0f, 0.0f}, {1.35f, 0.0f}, {0.0f, 1.35f}}, 0.17f, 0.73f, 0.35f));
		pieces.push_back(createPiece({{-0.62f, -0.62f}, {0.62f, -0.62f}, {0.62f, 0.62f}, {-0.62f, 0.62f}}, 0.12f, 0.8f, 0.78f));
		pieces.push_back(createPiece({{-0.82f, -0.54f}, {0.56f, -0.54f}, {0.90f, 0.54f}, {-0.48f, 0.54f}}, 0.92f, 0.47f, 0.7f));

		auto createTemplate = [](float r, float g, float b, const std::array<PiecePose, 5>& poses) {
			TargetTemplate t;
			t.frameColor[0] = r; t.frameColor[1] = g; t.frameColor[2] = b;
			t.poses = poses;
			return t;
		};

		templates.push_back(createTemplate(0.34f, 0.56f, 0.90f, {{
			PiecePose{{3.20f, -1.55f}, 180.0f, 0.58f},
			PiecePose{{5.10f, -1.55f}, 180.0f, 0.58f},
			PiecePose{{4.15f, 0.05f}, 315.0f, 0.58f},
			PiecePose{{3.35f, 1.55f}, 45.0f, 0.48f},
			PiecePose{{5.00f, 1.55f}, 315.0f, 0.48f}
		}}));

		templates.push_back(createTemplate(0.90f, 0.49f, 0.18f, {{
			PiecePose{{3.15f, -1.40f}, 90.0f, 0.58f},
			PiecePose{{5.15f, -1.40f}, 270.0f, 0.58f},
			PiecePose{{4.15f, 0.05f}, 0.0f, 0.58f},
			PiecePose{{3.35f, 1.50f}, 45.0f, 0.48f},
			PiecePose{{4.95f, 1.50f}, 315.0f, 0.48f}
		}}));

		templates.push_back(createTemplate(0.44f, 0.78f, 0.24f, {{
			PiecePose{{3.20f, -1.50f}, 180.0f, 0.58f},
			PiecePose{{5.10f, -1.50f}, 180.0f, 0.58f},
			PiecePose{{4.15f, 0.00f}, 315.0f, 0.58f},
			PiecePose{{3.35f, 1.50f}, 135.0f, 0.48f},
			PiecePose{{5.00f, 1.50f}, 225.0f, 0.48f}
		}}));
	}

	void resetLevel(int level) {
		currentLevel = level;
		selectedPiece = -1;
		isDragging = false;
		gameOver = false;
		gameWonFlash = false;
		extraTimeUsed = false;
		flashTimer = 0.0f;
		timeRemaining = 60.0f;
		assignLevelTargets();
	}

	void assignLevelTargets() {
		timeRemaining = std::max(12.0f, 62.0f - (currentLevel - 1) * 1.6f);
		gameWonFlash = true;
		flashTimer = 0.0f;
		const TargetTemplate& model = templates[(currentLevel - 1) % templates.size()];
		const std::array<Vec2, 5> startSlots = {
			Vec2{-8.5f, 3.0f},
			Vec2{-8.5f, 1.5f},
			Vec2{-8.5f, 0.0f},
			Vec2{-8.5f, -1.5f},
			Vec2{-8.5f, -3.0f}
		};

		for (size_t i = 0; i < pieces.size(); ++i) {
			Piece& piece = pieces[i];
			const PiecePose& pose = model.poses[i];
			piece.targetPosition = pose.position;
			piece.targetRotation = pose.rotation;
			piece.targetScale = pose.scale;
			piece.position = startSlots[i];
			piece.rotation = randomRange(0.0f, 359.0f);
			piece.scale = std::max(minScale, piece.targetScale * randomRange(0.78f, 0.92f));
			piece.snapped = false;
		}
	}

	void trySnapPiece(Piece& piece) {
		if (isPieceAtTarget(piece)) {
			piece.snapToTarget();
		}
	}

	bool isPieceAtTarget(const Piece& piece) const {
		float dx = piece.position.x - piece.targetPosition.x;
		float dy = piece.position.y - piece.targetPosition.y;
		float dist = std::sqrt(dx * dx + dy * dy);
		float rotDiff = Piece::angleDistance(piece.rotation, piece.targetRotation);
		float scaleDiff = std::fabs(piece.scale - piece.targetScale);
		return dist <= snapPosDist && rotDiff <= snapRotDist && scaleDiff <= snapScaleDist;
	}

	void checkLevelCompletion() {
		if (gameOver) {
			return;
		}

		for (const auto& piece : pieces) {
			if (!piece.snapped) {
				return;
			}
		}

		++currentLevel;
		assignLevelTargets();
	}

	void drawString(float x, float y, void* font, const std::string& text, float r, float g, float b) const {
		glColor3f(r, g, b);
		glRasterPos2f(x, y);
		for (char c : text) {
			glutBitmapCharacter(font, c);
		}
	}

	void drawPiece(const Piece& piece, bool target) const {
		const std::vector<Vec2> polygon = target ? piece.targetShape() : piece.worldShape();

		if (target) {
			glColor4f(0.25f, 0.25f, 0.25f, 0.25f);
		} else {
			float shade = piece.snapped ? 0.45f : 1.0f;
			glColor3f(piece.color[0] * shade, piece.color[1] * shade, piece.color[2] * shade);
		}

		glBegin(GL_POLYGON);
		for (const auto& vertex : polygon) {
			glVertex2f(vertex.x, vertex.y);
		}
		glEnd();

		glColor3f(0.0f, 0.0f, 0.0f);
		glLineWidth(2.0f);
		glBegin(GL_LINE_LOOP);
		for (const auto& vertex : polygon) {
			glVertex2f(vertex.x, vertex.y);
		}
		glEnd();
	}

	void drawBoard() const {
		glColor3f(borderColorR, borderColorG, borderColorB);
		glLineWidth(2.0f);
		glBegin(GL_LINES);
		glVertex2f(0.0f, -5.625f);
		glVertex2f(0.0f, 5.625f);
		glEnd();

		drawString(-9.4f, 4.9f, GLUT_BITMAP_HELVETICA_18, "Area de montagem das pecas", textColorR, textColorG, textColorB);
		drawString(1.2f, 4.9f, GLUT_BITMAP_HELVETICA_18, "Silhueta alvo da fase", textColorR, textColorG, textColorB);
	}

	void drawTargetFrame() const {
		const TargetTemplate& model = templates[(currentLevel - 1) % templates.size()];
		glColor3f(model.frameColor[0], model.frameColor[1], model.frameColor[2]);
		glLineWidth(8.0f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(2.75f, -2.65f);
		glVertex2f(6.85f, -2.65f);
		glVertex2f(6.85f, 2.65f);
		glVertex2f(2.75f, 2.65f);
		glEnd();

		glColor3f(0.98f, 0.98f, 0.99f);
		glLineWidth(2.0f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(2.95f, -2.45f);
		glVertex2f(6.65f, -2.45f);
		glVertex2f(6.65f, 2.45f);
		glVertex2f(2.95f, 2.45f);
		glEnd();
	}

	void drawTargets() const {
		drawTargetFrame();
		for (const auto& piece : pieces) {
			drawPiece(piece, true);
		}
	}

	void drawPieces() const {
		for (size_t i = 0; i < pieces.size(); ++i) {
			if (static_cast<int>(i) != selectedPiece) {
				drawPiece(pieces[i], false);
			}
		}

		if (selectedPiece >= 0) {
			drawPiece(pieces[selectedPiece], false);
		}
	}

	void drawHud() const {
		std::ostringstream hud;
		hud << "Fase: " << currentLevel << "    Tempo: " << std::fixed << std::setprecision(1) << std::max(0.0f, timeRemaining) << "s";
		drawString(-9.4f, -5.1f, GLUT_BITMAP_HELVETICA_18, hud.str(), textColorR, textColorG, textColorB);

		drawString(-9.4f, -5.45f, GLUT_BITMAP_HELVETICA_12,
				   "Controles: Mouse arrasta | R gira | + e - escalam | N reinicia na fase 1",
				   textColorR, textColorG, textColorB);

		drawTimeButton();
	}

	void drawOverlay() const {
		if (gameOver) {
			drawString(-2.6f, 0.2f, GLUT_BITMAP_TIMES_ROMAN_24, "Tempo esgotado! Voce morreu.", 0.85f, 0.07f, 0.07f);
			drawString(-2.4f, -0.3f, GLUT_BITMAP_HELVETICA_18, "Pressione N para reiniciar.", 0.35f, 0.09f, 0.09f);
		} else if (gameWonFlash) {
			drawString(-1.9f, 0.2f, GLUT_BITMAP_TIMES_ROMAN_24, "Fase completa! Proxima fase...", 0.09f, 0.5f, 0.16f);
		}
	}

	bool isInsideTimeButton(const Vec2& point) const {
		return point.x >= timeButtonLeft && point.x <= timeButtonRight && point.y >= timeButtonBottom && point.y <= timeButtonTop;
	}

	void drawTimeButton() const {
		if (extraTimeUsed) {
			glColor3f(0.78f, 0.79f, 0.82f);
		} else {
			glColor3f(0.20f, 0.55f, 0.95f);
		}

		glBegin(GL_QUADS);
		glVertex2f(timeButtonLeft, timeButtonBottom);
		glVertex2f(timeButtonRight, timeButtonBottom);
		glVertex2f(timeButtonRight, timeButtonTop);
		glVertex2f(timeButtonLeft, timeButtonTop);
		glEnd();

		glColor3f(1.0f, 1.0f, 1.0f);
		glLineWidth(2.0f);
		glBegin(GL_LINE_LOOP);
		glVertex2f(timeButtonLeft, timeButtonBottom);
		glVertex2f(timeButtonRight, timeButtonBottom);
		glVertex2f(timeButtonRight, timeButtonTop);
		glVertex2f(timeButtonLeft, timeButtonTop);
		glEnd();

		drawString(-9.15f, -4.08f, GLUT_BITMAP_HELVETICA_18, extraTimeUsed ? "Tempo usado" : "+15s", 1.0f, 1.0f, 1.0f);
		drawString(-9.2f, -4.32f, GLUT_BITMAP_HELVETICA_12, "botao unico", 1.0f, 1.0f, 1.0f);
	}
};

int main(int argc, char** argv) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowSize(PuzzleGame::windowWidth, PuzzleGame::windowHeight);
	glutCreateWindow("Puzzle de Formas - Tangram Infinito");

	PuzzleGame::instance().initialize();

	glutDisplayFunc(&PuzzleGame::displayThunk);
	glutKeyboardFunc(&PuzzleGame::keyboardThunk);
	glutMouseFunc(&PuzzleGame::mouseThunk);
	glutMotionFunc(&PuzzleGame::motionThunk);
	glutTimerFunc(16, &PuzzleGame::timerThunk, 0);

	glutMainLoop();
	return 0;
}
