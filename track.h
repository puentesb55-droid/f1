#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <vector>
#include <algorithm>
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "f1_types.h"

// ═════════════════════════════════════════════════════════════════════════
//  SECCIÓN 1: FUNCIONES AUXILIARES DE CONSTRUCCIÓN GEOMÉTRICA (BUILDERS)
//  Estas funciones calculan matemáticamente los puntos
//  (nodos) que componen el circuito usando vectores, trigonometría y matrices.
// ═════════════════════════════════════════════════════════════════════════

/**
 *  Añade tramos rectos a la pista calculando los puntos secuencialmente.
 * Recibe la posición actual (px, pz) y la dirección
 * angular (dir). Usando el seno y el coseno, calcula el vector de desplazamiento 
 * para avanzar hacia adelante de manera lineal dividiendo la longitud en sub-puntos (pts).
 */
static void addRecta(std::vector<NodoPista>& out,
    float& px, float& pz, float& dir,
    float longitud, float velLim, float ancho, int pts = 20)
{
    // dt y dz representan el vector director unitario hacia donde mira la recta en el plano XZ
    float dx = sinf(dir), dz = cosf(dir);
    float step = longitud / (float)(pts); // Distancia entre cada nodo de la recta

    for (int i = 0; i <= pts; i++) {
        float x = px + dx * i * step;
        float z = pz + dz * i * step;

        NodoPista nd;
        nd.centro = {x, 0.f, z}; // Altura inicial en 0 (Y = 0)
        nd.tangente = Vector3Normalize({dx, 0.f, dz}); // Vector hacia adelante
        nd.lateral = Vector3Normalize({dz, 0.f, -dx});  // Vector ortogonal (hacia el lado de la pista)
        nd.anchoLocal = ancho;
        nd.inclinacion = 0.f;  // Una recta no tiene peralte (inclinación)
        nd.curvatura = 0.f;    // Curvatura cero porque no es un arco
        nd.esCurva = false;
        nd.radio = 999.f;      // Un radio infinito matemáticamente representa una recta
        nd.limitVelocidad = velLim;

        out.push_back(nd); // Guarda el nodo en la lista
    }

    // Actualiza la posición de control global para que el siguiente tramo comience donde terminó este
    px += dx * longitud;
    pz += dz * longitud;
}

/** Añade curvas perfectas basadas en arcos de circunferencia.
 * Aplica trigonometría radial. Dependiendo de si el
 * ángulo es positivo o negativo, gira hacia la derecha o izquierda. Además, calcula de forma 
 * dinámica un límite de velocidad lógico: a menor radio (curva más cerrada), la IA frenará más.
 */
static void addCurva(std::vector<NodoPista>& out,
    float& px, float& pz, float& dir,
    float radio, float angDeg,
    float velRecta, float ancho,
    int pts = 90)
{
    // Convierte el ángulo de grados a radianes
    float angRad = angDeg * (float)M_PI / 180.f;
    if (fabsf(angRad) < 0.0001f) return;

    // Determina el sentido del giro: 1.0 = Derecha, -1.0 = Izquierda
    float sg = (angRad >= 0.f) ? 1.f : -1.f;
    float R = radio;
    float curv = 1.f / R; // La curvatura matemática es la inversa del radio

    // Lógica adaptativa de velocidad: Curvas cerradas reducen agresivamente la velocidad límite de la IA
    float velLim = velRecta * std::max(0.22f, std::min(0.96f, R / 50.f));

    // Guarda los puntos de origen antes de empezar a iterar sobre el arco
    float startX = px;
    float startZ = pz;
    float startDir = dir;

    for (int i = 0; i <= pts; i++) {
        float frac = (float)i / pts;       // Progreso actual de la curva (0.0 a 1.0)
        float theta = startDir + angRad * frac; // Ángulo actual en este nodo específico

        // Fórmulas paramétricas de la circunferencia para mover el punto sobre el arco texturizado
        float qx = startX + (R / sg) * (cosf(startDir) - cosf(theta));
        float qz = startZ + (R / sg) * (sinf(theta) - sinf(startDir));
        float tdx = sinf(theta);
        float tdz = cosf(theta);

        NodoPista nd;
        nd.centro = {qx, 0.f, qz};
        nd.tangente = Vector3Normalize({tdx, 0.f, tdz});
        nd.lateral = Vector3Normalize({tdz, 0.f, -tdx});
        nd.anchoLocal = ancho;
        // Agrega peralte automático: las curvas más cerradas se inclinan hacia adentro para simular apoyo físico
        nd.inclinacion = std::min(curv * 0.10f, 0.10f); 
        nd.curvatura = curv;
        nd.esCurva = true;
        nd.radio = R;
        nd.limitVelocidad = velLim;

        out.push_back(nd);
    }

    // Actualiza los puntos de control del mapa con la salida final de la curva
    dir = startDir + angRad;
    px = startX + (R / sg) * (cosf(startDir) - cosf(dir));
    pz = startZ + (R / sg) * (sinf(dir) - sinf(startDir));
}

/**
 * Cierra el circuito uniendo de forma recta el último punto con el origen (nodo 0).
 * Esto garantiza que los circuitos siempre se conecten
 * perfectamente sin importar que los cálculos manuales tengan pequeños errores de decimales.
 */
static void cerrarCircuito(std::vector<NodoPista>& out,
    float& px, float& pz, float& dir,
    float velLim, float ancho)
{
    if (out.empty()) return;

    Vector3 inicio = out.front().centro; // Obtiene el primer punto del circuito
    float dx = inicio.x - px;
    float dz = inicio.z - pz;
    float dist = sqrtf(dx * dx + dz * dz); // Distancia euclidiana restante

    if (dist < 0.25f) { // Si ya está lo suficientemente cerca, fuerza el cierre directo
        px = inicio.x;
        pz = inicio.z;
        return;
    }

    dir = atan2f(dx, dz); // Ajusta la dirección final hacia el punto inicial
    int pts = std::max(8, (int)(dist / 4.0f)); // Modula los puntos según la distancia de cierre
    addRecta(out, px, pz, dir, dist, velLim, ancho, pts);
}

/**
 * Busca cuál es el nodo de la pista más cercano a un vehículo en el espacio 3D.
 * Es una función crítica de optimización espacial. Realiza un
 * escaneo lineal calculando la distancia euclidiana entre la posición del auto y cada nodo. Devuelve 
 * el índice del nodo más cercano. Esto permite saber si el auto está en pista, en curva, o su velocidad límite.
 */
static int GetNodeIndex(const Auto& a, const std::vector<NodoPista>& nodos) {
    if (nodos.empty()) return 0;
    int mejorIdx = 0;
    float minDist = 999999.f;
    for (int i = 0; i < (int)nodos.size(); i++) {
        float d = Vector3Distance(a.pos3D, nodos[i].centro);
        if (d < minDist) {
            minDist = d;
            mejorIdx = i;
        }
    }
    return mejorIdx;
}

// ═════════════════════════════════════════════════════════════════════════
//  SECCIÓN 2: CLASE PISTABUILDER (CATÁLOGO DE CIRCUITOS)
//  Almacena las plantillas del diseño de trazados.
//  Combina llamadas consecutivas a addRecta y addCurva pasándose las variables
//  por referencia (px, pz, dir) para tejer el mapa de forma fluida.
// ═════════════════════════════════════════════════════════════════════════
class PistaBuilder {
public:

    static std::vector<NodoPista> BuildMonaco() {
        std::vector<NodoPista> nodos;
        float px = 0, pz = 0, dir = (float)M_PI / 2.0f;
        float W = 14.0f, VR = 70.0f; // W = Ancho de pista, VR = Velocidad en recta de referencia

        addRecta(nodos, px, pz, dir, 95, VR, W, 22);
        addCurva(nodos, px, pz, dir, 28, 90, VR, W, 30);
        addRecta(nodos, px, pz, dir, 70, VR, W, 18);
        addCurva(nodos, px, pz, dir, 28, 90, VR, W, 30);
        addRecta(nodos, px, pz, dir, 40, VR, W, 10);
        addCurva(nodos, px, pz, dir, 18, -28, VR, W, 14); // Curva negativa (izquierda)
        addCurva(nodos, px, pz, dir, 18, 28, VR, W, 14);  // Curva positiva (derecha)
        addRecta(nodos, px, pz, dir, 55, VR, W, 14);
        addCurva(nodos, px, pz, dir, 28, 90, VR, W, 30);
        addRecta(nodos, px, pz, dir, 70, VR, W, 18);
        addCurva(nodos, px, pz, dir, 28, 90, VR, W, 30);

        return nodos;
    }

    static std::vector<NodoPista> BuildSilverstone() {
        std::vector<NodoPista> nodos;
        float px = 0, pz = 0, dir = 0;
        float W = 14.f, VR = 95.f;

        addRecta(nodos, px, pz, dir, 60, VR, W, 12);
        addCurva(nodos, px, pz, dir, 50, -90, VR, W, 20);
        addRecta(nodos, px, pz, dir, 18, VR, W, 8);
        addCurva(nodos, px, pz, dir, 42, 60, VR, W, 18);
        addCurva(nodos, px, pz, dir, 36, -68, VR, W, 18);
        addCurva(nodos, px, pz, dir, 48, 38, VR, W, 14);
        addRecta(nodos, px, pz, dir, 48, VR, W, 10);
        addCurva(nodos, px, pz, dir, 40, -88, VR, W, 20);
        addRecta(nodos, px, pz, dir, 20, VR, W, 8);
        addCurva(nodos, px, pz, dir, 28, -50, VR, W, 16);
        addRecta(nodos, px, pz, dir, 10, VR, W, 6);
        addCurva(nodos, px, pz, dir, 32, -52, VR, W, 16);
        addRecta(nodos, px, pz, dir, 18, VR, W, 8);
        addCurva(nodos, px, pz, dir, 56, 90, VR, W, 20);
        addRecta(nodos, px, pz, dir, 14, VR, W, 6);
        addCurva(nodos, px, pz, dir, 36, -40, VR, W, 16);
        addRecta(nodos, px, pz, dir, 10, VR, W, 6);
        addCurva(nodos, px, pz, dir, 24, 80, VR, W, 18);
        addCurva(nodos, px, pz, dir, 20, -100, VR, W, 20);
        addRecta(nodos, px, pz, dir, 28, VR, W, 8);
        addCurva(nodos, px, pz, dir, 33, 60, VR, W, 16);
        addRecta(nodos, px, pz, dir, 38, VR, W, 10);
        cerrarCircuito(nodos, px, pz, dir, VR, W);

        return nodos;
    }

    static std::vector<NodoPista> BuildSuzuka() {
        std::vector<NodoPista> nodos;
        float px = 0, pz = 0, dir = 0;
        float W = 11.f, VR = 90.f;

        addRecta(nodos, px, pz, dir, 46, VR, W, 10);
        addCurva(nodos, px, pz, dir, 32, -58, VR, W, 18);
        addCurva(nodos, px, pz, dir, 32, 58, VR, W, 18);
        addRecta(nodos, px, pz, dir, 12, VR, W, 6);
        addCurva(nodos, px, pz, dir, 26, 90, VR, W, 18);
        addRecta(nodos, px, pz, dir, 10, VR, W, 6);
        addCurva(nodos, px, pz, dir, 20, -78, VR, W, 16);
        addRecta(nodos, px, pz, dir, 8, VR, W, 5);
        addCurva(nodos, px, pz, dir, 18, 53, VR, W, 14);
        addRecta(nodos, px, pz, dir, 18, VR, W, 6);
        addCurva(nodos, px, pz, dir, 12, -172, VR, W, 24);
        addRecta(nodos, px, pz, dir, 22, VR, W, 8);
        addCurva(nodos, px, pz, dir, 42, 83, VR, W, 18);
        addRecta(nodos, px, pz, dir, 36, VR, W, 10);
        addCurva(nodos, px, pz, dir, 76, -38, VR, W, 18);
        addRecta(nodos, px, pz, dir, 14, VR, W, 6);
        addCurva(nodos, px, pz, dir, 14, 53, VR, W, 14);
        addCurva(nodos, px, pz, dir, 14, -53, VR, W, 14);
        addRecta(nodos, px, pz, dir, 36, VR, W, 10);
        cerrarCircuito(nodos, px, pz, dir, VR, W);

        return nodos;
    }
};

// ═════════════════════════════════════════════════════════════════════════
//  SECCIÓN 3: CLASE PISTARENDERER (ENTORNO GRÁFICO Y GEOMETRÍA 3D)
//  Es el motor gráfico de la pista. Genera de forma
//  procedimental (usando semillas matemáticas fijas) los entornos, árboles,
//  gradas, edificios y pinta los triángulos 3D de la pista con sus límites.
// ═════════════════════════════════════════════════════════════════════════
class PistaRenderer {
public:

    /**
     * Renderiza la decoración del mapa de forma algorítmica.
     * Calcula el centro de masa del circuito (AABB) 
     * para proyectar el césped infinito de fondo. Usa "srand(semilla)" para que los 
     * elementos procedimentales (árboles, edificios de Mónaco o tribunas de Silverstone) 
     * se generen en ubicaciones idénticas en cada fotograma sin consumir memoria en disco.
     */
    static void DrawPaisaje(const std::vector<NodoPista>& nodos, PistaID id, Clima clima)
    {
        // Pinta el gran plano verde de base (césped)
        Color grass = (id == PistaID::MONACO) ? Color{20,65,20,255} : Color{25,80,25,255};
        DrawPlane({0, -0.12f, 0}, {2000, 2000}, grass);

        // Algoritmo AABB (Axis-Aligned Bounding Box) para encontrar los límites y el centro del circuito
        float minX = 9999, maxX = -9999, minZ = 9999, maxZ = -9999;
        for (auto& nd : nodos) {
            minX = std::min(minX, nd.centro.x); 
            maxX = std::max(maxX, nd.centro.x);
            minZ = std::min(minZ, nd.centro.z); 
            maxZ = std::max(maxZ, nd.centro.z);
        }
        float cx = (minX + maxX) / 2, cz = (minZ + maxZ) / 2;
        float rx = (maxX - minX) / 2, rz = (maxZ - minZ) / 2;
        float rMax = std::max(rx, rz);

        // ANILLO DE ÁRBOLES EXTERNO: Genera 100 árboles de forma circular rodeando el mapa
        srand(42); // Semilla fija para consistencia visual
        for (int i = 0; i < 100; i++) {
            float angle = (float)i / 100 * 2 * (float)M_PI;
            float jitter = (float)(rand() % 30); // Variación aleatoria controlada de distancia
            float dist = rMax + 18 + jitter;
            float tx = cx + cosf(angle) * dist;
            float tz = cz + sinf(angle) * dist * (rz / std::max(rx, 1.f));
            float h = 3.f + (float)(rand() % 5);
            DrawCylinder({tx, 0, tz}, 0.20f, 0.14f, h * 0.42f, 6, {72, 44, 16, 255}); // Tronco
            Color vc = {(unsigned char)(16 + rand() % 28), (unsigned char)(85 + rand() % 42), (unsigned char)(12 + rand() % 22), 255};
            DrawCylinder({tx, h * 0.36f, tz}, 1.2f + (rand() % 8) * 0.1f, 0.04f, h * 0.7f, 8, vc); // Hojas
        }

        // ANILLO DE ÁRBOLES INTERNO (Solo en zonas libres del centro)
        for (int i = 0; i < 50; i++) {
            float angle = (float)i / 50 * 2 * (float)M_PI;
            float dist = std::min(rx, rz) * 0.30f * (0.5f + (float)(rand() % 80) / 200.f);
            float tx = cx + cosf(angle) * dist;
            float tz = cz + sinf(angle) * dist;
            float h = 2.5f + (float)(rand() % 4);
            DrawCylinder({tx, 0, tz}, 0.16f, 0.10f, h * 0.38f, 6, {75, 44, 16, 255});
            Color vc = {(unsigned char)(20 + rand() % 22), (unsigned char)(88 + rand() % 38), (unsigned char)(14 + rand() % 18), 255};
            DrawCylinder({tx, h * 0.32f, tz}, 1.0f, 0.03f, h * 0.66f, 7, vc);
        }

        // ELEMENTO TEMÁTICO 1: Si es Mónaco, genera rascacielos y edificios urbanos en el fondo
        if (id == PistaID::MONACO) {
            srand(13);
            for (int b = 0; b < 28; b++) {
                float a = (float)b / 28 * 2 * (float)M_PI;
                float tx = cx + cosf(a) * (rx + 28 + (rand() % 22));
                float tz = cz + sinf(a) * (rz + 28 + (rand() % 22));
                float bh = 7 + (rand() % 15); // Altura del edificio
                float bw = 4 + (rand() % 7);  // Anchura del edificio
                Color bc = {(unsigned char)(148 + rand() % 70), (unsigned char)(140 + rand() % 70), (unsigned char)(130 + rand() % 70), 255};
                DrawCube({tx, bh / 2, tz}, (float)bw, bh, (float)(3 + rand() % 5), bc); // Bloque estructura
                for (int f = 0; f < (int)(bh / 2.2f); f++) // Cristales de las ventanas reflectantes
                    DrawCube({tx, 1.8f + f * 2.2f, tz}, (float)bw + 0.06f, 0.65f, 0.08f, {185, 212, 255, 150});
            }
        }

        // ELEMENTO TEMÁTICO 2: Si es Silverstone o Suzuka, renderiza gradas masivas de espectadores
        if (id != PistaID::MONACO) {
            for (int t = 0; t < 6; t++) {
                float a = (float)t / 6 * 2 * (float)M_PI;
                float tx = cx + cosf(a) * (rx + 10);
                float tz = cz + sinf(a) * (rz + 10);
                DrawCube({tx, 1.2f, tz}, 22, 2.4f, 5.5f, {165, 170, 192, 255}); // Grada base
                DrawCube({tx, 4.0f, tz}, 22.5f, 0.28f, 6.0f, {65, 68, 92, 255}); // Techo de la grada
                for (int s = 0; s < 7; s++) // Columnas de soporte estructural
                    DrawCube({tx - 10.5f + s * 3.5f, 2.4f, tz}, 0.12f, 2.2f, 5.2f, {50, 54, 75, 255});
            }
        }
    }

    /**
     * Construye la malla 3D de la pista uniendo los nodos mediante primitivas de triángulos.
     * Recorre los nodos en parejas (A y B). Usando el vector lateral
     * de cada nodo, extrae los extremos izquierdos y derechos de la carretera. Con estos extremos forma 
     * los triángulos 3D del asfalto, las zonas de escape (runoffs), los pianos bicolores en curvas y 
     * muros físicos verticales para dar volumen volumétrico 3D al juego.
     */
    static void Draw(const std::vector<NodoPista>& nodos, PistaID id) {
        int n = (int)nodos.size();
        if (n < 2) return;

        // Desactiva el descarte de caras traseras para que los muros se vean por dentro y fuera
        rlDisableBackfaceCulling();

        // Configuración de la paleta cromática según el circuito
        Color asf1   = {28, 28, 34, 255}; 
        Color asf2   = {35, 35, 42, 255}; 
        Color kRojo  = (id == PistaID::MONACO) ? Color{205, 15, 15, 255} : Color{210, 65, 10, 255};
        Color kBlanc = {234, 234, 234, 255};
        Color lineaC = {255, 255, 255, 230};
        Color lineaB = {255, 255, 255, 255}; 
        Color muro   = (id == PistaID::MONACO) ? Color{145, 145, 150, 255} : Color{238, 238, 238, 255};
        Color runoff = {238, 238, 238, 255}; 

        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n; // El nodo B es el siguiente index (con aritmética modular para conectar el fin con el inicio)
            const NodoPista& A = nodos[i];
            const NodoPista& B = nodos[j];

            float wA = A.anchoLocal * 0.5f, wB = B.anchoLocal * 0.5f;

            // Vectores directores rotados en base a la inclinación (peralte) del circuito
            Vector3 up = {0, 1, 0};
            Vector3 latA = Vector3Normalize(Vector3Add(Vector3Scale(A.lateral, cosf(A.inclinacion)), Vector3Scale(up, sinf(A.inclinacion))));
            Vector3 latB = Vector3Normalize(Vector3Add(Vector3Scale(B.lateral, cosf(B.inclinacion)), Vector3Scale(up, sinf(B.inclinacion))));

            // Coordenadas exactas de los bordes del asfalto (Izquierda y Derecha)
            Vector3 AL = Vector3Add(A.centro, Vector3Scale(latA,  wA)); AL.y = 0.050f;
            Vector3 AR = Vector3Add(A.centro, Vector3Scale(latA, -wA)); AR.y = 0.050f;
            Vector3 BL = Vector3Add(B.centro, Vector3Scale(latB,  wB)); BL.y = 0.050f;
            Vector3 BR = Vector3Add(B.centro, Vector3Scale(latB, -wB)); BR.y = 0.050f;

            // Coordenadas de los bordes externos de escape (Runoff) añadiendo metros extra (rf)
            float rf = 3.0f;
            Vector3 RL = Vector3Add(A.centro, Vector3Scale(latA,  wA + rf)); RL.y = 0.0f;
            Vector3 RR = Vector3Add(A.centro, Vector3Scale(latA, -wA - rf)); RR.y = 0.0f;
            Vector3 SL = Vector3Add(B.centro, Vector3Scale(latB,  wB + rf)); SL.y = 0.0f;
            Vector3 SR = Vector3Add(B.centro, Vector3Scale(latB, -wB - rf)); SR.y = 0.0f;

            // Dibuja las zonas de escape grises texturizadas con dos triángulos por lado
            DrawTriangle3D(RL, AL, BL, runoff);
            DrawTriangle3D(RL, BL, SL, runoff);
            DrawTriangle3D(AR, RR, BR, runoff);
            DrawTriangle3D(RR, SR, BR, runoff);

            // ── EXTENSIÓN: GENERACIÓN DE MUROS DE CONTENCIÓN LATERALES VERTICALES ──
            // Genera una pared vertical 3D levantando coordenadas paralelas en el eje Y (mh)
            float mh = 1.1f; 
            Vector3 ML_Alta = {RL.x, mh, RL.z};
            Vector3 MR_Alta = {RR.x, mh, RR.z};
            Vector3 NL_Alta = {SL.x, mh, SL.z};
            Vector3 NR_Alta = {SR.x, mh, SR.z};
            DrawTriangle3D(RL, NL_Alta, SL, muro); // Muro Izquierdo cara 1
            DrawTriangle3D(RL, ML_Alta, NL_Alta, muro); // Muro Izquierdo cara 2
            DrawTriangle3D(RR, SR, NR_Alta, muro); // Muro Derecho cara 1
            DrawTriangle3D(RR, NR_Alta, MR_Alta, muro); // Muro Derecho cara 2

            // Dibuja el asfalto intercalando dos colores oscuros para dar sensación de velocidad por tramos
            Color col = (i / 6 % 2 == 0) ? asf1 : asf2;
            DrawTriangle3D(AL, AR, BL, col);
            DrawTriangle3D(AR, BR, BL, col);
                       
            // ── GENERACIÓN DE PIANOS (KERBS) EN LAS CURVAS ──
            if (A.esCurva || B.esCurva) {
                float kw = A.anchoLocal * 0.095f; // Anchura del piano proporcional
                float ky = 0.020f;                // Altura leve sobre el suelo
                Color kc = (i / 4 % 2 == 0) ? kRojo : kBlanc; // Patrón alternante rojo y blanco

                // Piano del extremo izquierdo
                Vector3 KLA = Vector3Add(AL, Vector3Scale(latA, kw)); KLA.y = AL.y + ky;
                Vector3 KLB = Vector3Add(BL, Vector3Scale(latB, kw)); KLB.y = BL.y + ky;
                DrawTriangle3D(AL, BL, KLA, kc);
                DrawTriangle3D(KLA, BL, KLB, kc);

                // Piano del extremo derecho
                Vector3 KRA = Vector3Add(AR, Vector3Scale(latA, -kw)); KRA.y = AR.y + ky;
                Vector3 KRB = Vector3Add(BR, Vector3Scale(latB, -kw)); KRB.y = BR.y + ky;
                DrawTriangle3D(AR, KRA, BR, kc);
                DrawTriangle3D(KRA, KRB, BR, kc);
            }
                    
            // ── RENDERIZADO DE LÍNEAS BLANCAS DE BORDE DE PISTA ──
            float lw = 0.06f; // Espesor de la línea
            Vector3 LLA = Vector3Add(AL, Vector3Scale(latA, -lw)); LLA.y = AL.y + 0.005f;
            Vector3 LLB = Vector3Add(BL, Vector3Scale(latB, -lw)); LLB.y = BL.y + 0.005f;
            DrawTriangle3D(AL, LLA, LLB, lineaB);
            DrawTriangle3D(AL, LLB, BL, lineaB);

            // ── LÍNEAS DISCONTINUAS CENTRALES (EJE CENTRAL) ──
            if (i % 12 < 7) { // Crea espacios vacíos entre segmentos para simular la discontinuidad de la carretera
                float cw = 0.09f;  
                float yc = 0.025f; 

                Vector3 CLA = Vector3Add(A.centro, Vector3Scale(latA,  cw)); CLA.y = A.centro.y + yc;
                Vector3 CRA = Vector3Add(A.centro, Vector3Scale(latA, -cw)); CRA.y = A.centro.y + yc;
                Vector3 CLB = Vector3Add(B.centro, Vector3Scale(latB,  cw)); CLB.y = B.centro.y + yc;
                Vector3 CRB = Vector3Add(B.centro, Vector3Scale(latB, -cw)); CRB.y = B.centro.y + yc;

                DrawTriangle3D(CLA, CRA, CLB, lineaC);
                DrawTriangle3D(CRA, CRB, CLB, lineaC);
            }
        }
        rlEnableBackfaceCulling(); // Reactiva el sistema de optimización de renderizado por defecto
    }

    /**
     * Dibuja la línea de meta en formato de damero (cuadros blancos y negros).
     * Se posiciona exactamente sobre el Nodo 0 de la pista. 
     * Divide el ancho de la pista en pequeños segmentos (cuadros) de tamaño fijo (sw) y pinta 
     * alternando colores usando el operador mod (i % 2) para formar el clásico patrón de bandera de carreras.
     */
    static void DrawMeta(const std::vector<NodoPista>& nodos) {
        if (nodos.empty()) return;
        const NodoPista& nd = nodos[0]; 
        float w = nd.anchoLocal * 0.5f, sw = 0.52f; // sw = Tamaño de cada cuadro
        int cuadros = (int)(nd.anchoLocal / sw) + 1; 

        for (int i = 0; i < cuadros; i++) {
            float t0 = -w + i * sw, t1 = t0 + sw;
            Vector3 A = Vector3Add(nd.centro, Vector3Scale(nd.lateral, t0));
            Vector3 B = Vector3Add(nd.centro, Vector3Scale(nd.lateral, t1));
            Vector3 C = Vector3Add(A, Vector3Scale(nd.tangente, sw));
            Vector3 D = Vector3Add(B, Vector3Scale(nd.tangente, sw));
            
            float ym = 0.04f; A.y = B.y = C.y = D.y = ym; // Eleva un poco sobre el asfalto para evitar Z-Fighting
            Color col = (i % 2 == 0) ? WHITE : BLACK;     // Alternancia de color lógica
            DrawTriangle3D(A, C, B, col);
            DrawTriangle3D(B, C, D, col);
        }
    }
};
