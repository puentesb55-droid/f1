#pragma once
#include "f1_types.h"
#include "track.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <cstdio>

// ═══════════════════════════════════════════════════════════
//  CLASE RENDERER (SISTEMA GRÁFICO)
//  Explicación general: Esta clase es el motor gráfico encargado de 
//  proyectar y dibujar toda la parte visual del simulador en la pantalla 
//  (objetos 3D, efectos ambientales de clima, rastro de llantas y la interfaz HUD).
// ═══════════════════════════════════════════════════════════
class Renderer {
public:

    // ────────────────────────────────────────────────────────
    //  AUTO F1 REALISTA (Geometría procedural detallada)
    //  Explicación para tu exposición: En lugar de cargar un pesado modelo 3D
    //  externo (.obj o .fbx), esta función construye el monoplaza en tiempo de
    //  ejecución de forma procedural combinando primitivas geométricas (cubos y cilindros)
    //  y aplicando transformaciones espaciales basadas en matrices de rotación.
    // ────────────────────────────────────────────────────────
    static void DrawCarF1(const Auto& a) {
        // CONTROL DE ACCIDENTE: Si el auto volcó, se renderiza una caja estática y partículas de humo
        if (a.accidentado) {
            DrawCube(a.pos3D, CAR_LENGTH*0.8f, 0.5f, CAR_WIDTH*0.8f, {80,80,80,200});
            for (int s = 0; s < 5; s++) {
                // Ondas matemáticas sinusoidales dependientes del tiempo para animar el humo
                float ox = sinf(a.tiempoTotal*3+s)*1.5f;
                float oz = cosf(a.tiempoTotal*2+s)*1.5f;
                DrawSphere({a.pos3D.x+ox, a.pos3D.y+1.5f+s*0.3f, a.pos3D.z+oz},
                           0.6f+s*0.15f, {180,180,180,(unsigned char)(80-s*12)});
            }
            return;
        }

        // MATRICES DE TRANSFORMACIÓN (Ángulos de Euler: Yaw, Pitch, Roll)
        // Permiten orientar el auto en el espacio según la dirección de la pista y las físicas
        Matrix mYaw   = MatrixRotateY(a.yaw + (float)M_PI); // Rotación en el eje vertical Y (dirección)
        Matrix mRoll  = MatrixRotateZ(a.roll);             // Rotación en el eje longitudinal Z (peralte/derrape)
        Matrix mPitch = MatrixRotateX(a.pitch);            // Rotación en el eje lateral X (frenado/aceleración)
        
        // Multiplicación de matrices para combinar las 3 rotaciones en una única matriz final de transformación
        Matrix rot    = MatrixMultiply(MatrixMultiply(mPitch, mRoll), mYaw);

        // EXPRESIONES LAMBDA ([&]) PARA TRANSFORMACIONES LOCALES A GLOBALES
        // T y TC toman coordenadas locales del auto (donde (0,0,0) es el centro del coche) y las
        // transforman a coordenadas globales del mundo 3D multiplicándolas por la matriz de rotación.
        auto T = [&](float x, float y, float z) -> Vector3 {
            return Vector3Add(a.pos3D, Vector3Transform({x, y, z}, rot));
        };
        auto TC = [&](Vector3 v) -> Vector3 {
            return Vector3Add(a.pos3D, Vector3Transform(v, rot));
        };

        // Paleta de colores asignada dinámicamente según el piloto y el equipo
        Color body  = a.colorCuerpo;
        Color acent = a.colorAcento;
        Color negro = {12,12,12,255};
        Color gris  = {70,70,70,255};
        Color grisC = {140,140,140,255};
        Color cromo = {200,200,210,255};
        Color rojo  = {180,20,20,255};

        // CONSTRUCCIÓN PROCEDURAL DE LAS PARTES DEL VEHÍCULO
        // ── Suelo / fondo plano (difusor inferior del chasis)
        DrawCube(T(0, -0.08f, 0), CAR_LENGTH*0.9f, 0.06f, CAR_WIDTH*0.75f, negro);

        // ── Cuerpo central (Monocasco principal o habitáculo)
        DrawCube(T(0.2f, 0.12f, 0), CAR_LENGTH*0.72f, 0.22f, CAR_WIDTH*0.52f, body);

        // ── Pontones laterales (Sidepods para refrigeración aerodinámica)
        DrawCube(T(0.1f, 0.18f,  CAR_WIDTH*0.38f), CAR_LENGTH*0.52f, 0.28f, 0.62f, body);
        DrawCube(T(0.1f, 0.18f, -CAR_WIDTH*0.38f), CAR_LENGTH*0.52f, 0.28f, 0.62f, body);
        // Entradas de aire negras frontales de los pontones
        DrawCube(T(0.55f, 0.22f,  CAR_WIDTH*0.42f), 0.18f, 0.20f, 0.35f, negro);
        DrawCube(T(0.55f, 0.22f, -CAR_WIDTH*0.42f), 0.18f, 0.20f, 0.35f, negro);

        // ── Morro estructural (Nosecone frontal)
        DrawCube(T(1.70f, 0.05f, 0), 0.90f, 0.13f, 0.50f, acent);
        DrawCube(T(2.10f, 0.02f, 0), 0.55f, 0.09f, 0.32f, acent);
        DrawCube(T(2.38f, 0.00f, 0), 0.22f, 0.07f, 0.20f, negro);

        // ── Cockpit y visera del piloto
        DrawCube(T(-0.15f, 0.35f, 0), 1.35f, 0.25f, 0.58f, body);
        DrawCube(T(-0.05f, 0.50f, 0), 0.80f, 0.08f, 0.52f, {20,20,20,220}); // Cristal translúcido

        // ── DISPOSITIVO HALO (Estructura de seguridad de titanio para el piloto)
        DrawCube(T( 0.05f, 0.72f, 0),     1.05f, 0.055f, 0.055f, cromo);
        DrawCube(T( 0.30f, 0.56f,  0.29f), 0.10f, 0.38f,  0.055f, cromo);
        DrawCube(T( 0.30f, 0.56f, -0.29f), 0.10f, 0.38f,  0.055f, cromo);
        DrawCube(T(-0.42f, 0.64f, 0),     0.14f, 0.055f,  0.42f, cromo);

        // ── ALERÓN DELANTERO (Generación de carga aerodinámica en el eje frontal)
        DrawCube(T(2.18f, -0.04f, 0), 0.12f, 0.055f, CAR_WIDTH+0.55f, negro); // Plano principal
        DrawCube(T(2.10f,  0.06f, 0), 0.20f, 0.040f, CAR_WIDTH+0.40f, body);  // Flap superior 1
        DrawCube(T(2.02f,  0.14f, 0), 0.18f, 0.035f, CAR_WIDTH+0.28f, acent); // Flap superior 2
        // Placas extremas laterales (Endplates)
        DrawCube(T(2.08f,  0.05f,  (CAR_WIDTH+0.55f)*0.5f), 0.52f, 0.18f, 0.06f, body);
        DrawCube(T(2.08f,  0.05f, -(CAR_WIDTH+0.55f)*0.5f), 0.52f, 0.18f, 0.06f, body);

        // ── ALERÓN TRASERO (Soporte aerodinámico con alerón DRS y Beam wing)
        DrawCube(T(-1.90f, 0.78f, 0), 0.10f, 0.055f, CAR_WIDTH+0.42f, negro); // Plano base
        DrawCube(T(-1.82f, 0.91f, 0), 0.18f, 0.040f, CAR_WIDTH+0.35f, body);  // Flap DRS
        DrawCube(T(-1.78f, 0.48f, 0), 0.22f, 0.060f, CAR_WIDTH*0.55f, negro); // Beam wing
        // Soportes del alerón (Pilones traseros)
        DrawCube(T(-1.90f, 0.62f,  (CAR_WIDTH+0.42f)*0.5f), 0.14f, 0.52f, 0.055f, body);
        DrawCube(T(-1.90f, 0.62f, -(CAR_WIDTH+0.42f)*0.5f), 0.14f, 0.52f, 0.055f, body);

        // ── TUBO DE ESCAPE REVESTIDO (Simulación de incandescencia mediante color naranja)
        DrawCylinder(T(-1.98f, 0.30f, 0), 0.09f, 0.07f, 0.28f, 8, gris);
        DrawCylinder(T(-2.12f, 0.30f, 0), 0.07f, 0.07f, 0.04f, 8, {220,120,30,255}); // Punta térmica

        // ── CONTRALOR Y RENDERIZADO DE LAS 4 RUEDAS (Ejes X, Y, Z locales fijos)
        struct WP { float x,y,z; };
        WP wp[4] = {
            { 1.40f, -0.10f,  CAR_WIDTH*0.50f}, // Delantera Izquierda
            { 1.40f, -0.10f, -CAR_WIDTH*0.50f}, // Delantera Derecha
            {-1.40f, -0.10f,  CAR_WIDTH*0.50f}, // Trasera Izquierda
            {-1.40f, -0.10f, -CAR_WIDTH*0.50f}  // Trasera Derecha
        };
        for (auto& w : wp) {
            Vector3 wPos = T(w.x, w.y, w.z);
            DrawCylinder(wPos, WHEEL_R, WHEEL_R, 0.50f, 16, negro);           // Goma exterior
            DrawCylinder(wPos, WHEEL_R*0.50f, WHEEL_R*0.50f, 0.54f, 12, grisC); // Llanta de magnesio
            DrawCylinder(wPos, WHEEL_R*0.22f, WHEEL_R*0.22f, 0.56f, 8, acent); // Tuerca central
            
            // Tapacubos aerodinámicos (Obligatorios por reglamento de F1, solo en ruedas delanteras)
            if (w.x > 0) {
                DrawCylinder(wPos, WHEEL_R*1.02f, WHEEL_R*1.02f, 0.06f, 16, {body.r, body.g, body.b, 120});
            }
        }

        // ── Brazos de suspensión estructural (Pushrod / Pullrod)
        DrawCube(T( 1.55f, 0.05f,  0.52f), 0.55f, 0.04f, 0.04f, gris);
        DrawCube(T( 1.55f, 0.05f, -0.52f), 0.55f, 0.04f, 0.04f, gris);
        DrawCube(T(-1.55f, 0.05f,  0.52f), 0.55f, 0.04f, 0.04f, gris);
        DrawCube(T(-1.55f, 0.05f, -0.52f), 0.55f, 0.04f, 0.04f, gris);

        // ── Portanúmeros del piloto en los costados de los pontones
        DrawCube(T(0.2f, 0.32f,  CAR_WIDTH*0.46f+0.32f), 0.45f, 0.22f, 0.01f, WHITE);
        DrawCube(T(0.2f, 0.32f, -CAR_WIDTH*0.46f-0.32f), 0.45f, 0.22f, 0.01f, WHITE);

        // ── SISTEMA DE PARTÍCULAS: EFECTO DE HUMO POR DERRAPE
        // Si las llantas patinan, genera dinámicamente esferas translúcidas con desvanecimiento alfa (age)
        if (a.derrapeIntensidad > 0.18f) {
            int nPart = (int)(a.derrapeIntensidad * 10);
            for (int s = 0; s < nPart; s++) {
                float age   = (float)s / nPart;
                float ox   = sinf(a.tiempoTotal*5+s*1.3f) * a.derrapeIntensidad * 1.8f;
                float oz   = cosf(a.tiempoTotal*4+s*0.9f) * a.derrapeIntensidad * 1.8f;
                float sz   = 0.3f + age * a.derrapeIntensidad * 1.2f; // Crece el tamaño con el tiempo
                unsigned char al = (unsigned char)(100 * a.derrapeIntensidad * (1-age*0.6f)); // Desvanecimiento
                DrawSphere(T(-1.6f+ox*0.3f, 0.2f+age*0.8f, oz), sz, {210,210,210,al});
            }
        }
    }

    // ────────────────────────────────────────────────────────
    //  RASTRO DE NEUMÁTICOS (Skidmarks en el asfalto)
    //  Explicación para tu exposición: Dibuja líneas físicas 3D justo encima de la pista (`y + 0.01f`)
    //  para evitar el efecto óptico de parpadeo (*Z-Fighting*). Recorre el vector histórico de derrapes 
    //  guardado en el objeto Auto y conecta los puntos secuencialmente creando la marca de goma quemada.
    // ────────────────────────────────────────────────────────
    static void DrawTireMarks(const Auto& a) {
        int n = (int)a.rastroIzq.size();
        if (n < 2 || a.derrapeIntensidad < 0.1f) return;
        for (int i = 1; i < n; i++) {
            float age   = (float)i / n; // Controla la opacidad para que el rastro viejo se borre lentamente
            unsigned char al = (unsigned char)(60 * age * std::min(a.derrapeIntensidad, 1.0f));
            
            // Marca de rodamiento izquierda
            DrawLine3D(
                {a.rastroIzq[i-1].x, a.rastroIzq[i-1].y+0.01f, a.rastroIzq[i-1].z},
                {a.rastroIzq[i].x,   a.rastroIzq[i].y+0.01f,   a.rastroIzq[i].z},
                {20,20,20,al});
            // Marca de rodamiento derecha
            DrawLine3D(
                {a.rastroDer[i-1].x, a.rastroDer[i-1].y+0.01f, a.rastroDer[i-1].z},
                {a.rastroDer[i].x,   a.rastroDer[i].y+0.01f,   a.rastroDer[i].z},
                {20,20,20,al});
        }
    }

    // ────────────────────────────────────────────────────────
    //  SKYBOX / COLOR DE ENTORNO
    //  Explicación para tu exposición: Modifica el color de fondo de la pantalla utilizando la
    //  función ClearBackground de Raylib de acuerdo al enumerador meteorológico (Clima) de la simulación.
    // ────────────────────────────────────────────────────────
    static void DrawSkybox(Clima clima) {
        Color cielo;
        switch(clima) {
            case Clima::SOLEADO:  cielo={100,160,220,255}; break; // Azul brillante
            case Clima::NUBLADO:  cielo={110,120,135,255}; break; // Gris suave
            case Clima::LLUVIA:   cielo={55, 65, 80, 255}; break;  // Gris oscuro/plomizo
            case Clima::TORMENTA: cielo={28, 28, 42, 255}; break;  // Tonalidad violácea/nocturna
            default:              cielo={100,160,220,255};
        }
        ClearBackground(cielo);
    }

    // ────────────────────────────────────────────────────────
    //  EFECTO AMBIENTAL: LLUVIA 3D DINÁMICA
    //  Explicación para tu exposición: Es un sistema de partículas ambientales. Genera líneas 3D
    //  aleatorias rodeando el frustum de la cámara (`spread`) en cada cuadro. Al reiniciar la semilla (`srand`)
    //  con base en el tiempo de juego, las gotas dan el efecto óptico de caer velozmente con inclinación.
    // ────────────────────────────────────────────────────────
    static void DrawRain(Camera3D cam, Clima clima, float tiempo) {
        if (clima != Clima::LLUVIA && clima != Clima::TORMENTA) return;
        int drops = (clima == Clima::TORMENTA) ? 300 : 150; // Duplica las gotas si es tormenta
        float spread = 35.0f; // Radio del cubo de lluvia alrededor de la cámara del jugador
        srand((unsigned)(tiempo * 30)); // Semilla determinista basada en cuadros de tiempo

        for (int i = 0; i < drops; i++) {
            // Posiciones aleatorias relativas a la cámara activa
            float rx = cam.position.x + ((float)rand()/RAND_MAX - 0.5f) * spread;
            float ry = cam.position.y + ((float)rand()/RAND_MAX) * 18.0f;
            float rz = cam.position.z + ((float)rand()/RAND_MAX - 0.5f) * spread;
            float len = (clima == Clima::TORMENTA) ? 1.8f : 1.1f; // Trazado más largo en tormenta
            
            // Dibuja la traza inclinada de la gota
            DrawLine3D({rx, ry, rz}, {rx+0.08f, ry-len, rz+0.05f}, {170,190,255,140});
        }
    }

    // ────────────────────────────────────────────────────────
    //  HUD (Heads-Up Display / INTERFAZ 2D EN PANTALLA)
    //  Explicación para tu exposición: Dibuja toda la instrumentación digital en 2D superpuesta
    //  a los gráficos 3D. Muestra velocímetro, posiciones relativas, tiempos por vuelta y alertas de pérdida de control.
    // ────────────────────────────────────────────────────────
    static void DrawHUD(const ConfigCarrera& cfg, const Auto& player,
                        VistaCamera vista, int camTarget, float tiempo)
    {
        int W = GetScreenWidth();
        int H = GetScreenHeight();
        char buf[128]; // Buffer para formatear cadenas de texto mediante sprintf

        // ── Panel velocímetro (Esquina inferior derecha)
        DrawRectangleRounded({(float)(W-215),(float)(H-125),205,115}, 0.15f, 8, {0,0,0,170});
        DrawRectangleRoundedLines({(float)(W-215),(float)(H-125),205,115}, 0.15f, 8, {255,180,0,180});

        // Impresión de velocidad numérica
        sprintf(buf, "%3.0f", player.velocidadActual);
        DrawText(buf, W-200, H-118, 52, {255,210,0,255});
        DrawText("km/h", W-138, H-72, 14, {200,200,200,200});

        // Contador de vueltas del circuito
        sprintf(buf, "Vuelta  %d / %d", player.vuelta+1, cfg.vueltas);
        DrawText(buf, W-208, H-58, 16, WHITE);

        // Posición actual de carrera (Cambia de color según estés en el podio)
        sprintf(buf, "P%d", player.posicion);
        Color pCol = player.posicion==1 ? Color{255,220,0,255} :  // Oro para el Líder
                     player.posicion<=3 ? Color{100,200,100,255} : WHITE; // Verde para el podio
        DrawText(buf, W-208, H-38, 22, pCol);

        // Cronómetro de la vuelta en formato Minutos:Segundos.Milésimas
        int min=(int)(player.tiempoVuelta/60), sec=(int)fmodf(player.tiempoVuelta,60);
        int ms=(int)((player.tiempoVuelta-floorf(player.tiempoVuelta))*1000);
        sprintf(buf, "%d:%02d.%03d", min, sec, ms);
        DrawText(buf, W-145, H-38, 16, {160,160,255,255});

        // ── Cartel de Alerta por Derrape Agresivo (Centro de la pantalla)
        if (player.derrapeIntensidad > 0.2f) {
            unsigned char r = (unsigned char)(255 * player.derrapeIntensidad);
            unsigned char a2 = (unsigned char)(200 * player.derrapeIntensidad);
            DrawRectangle(W/2-70, H-75, 140, 32, {r,40,0,a2}); // Parpadeo rojo en función al patinaje
            DrawText("DERRAPE", W/2-MeasureText("DERRAPE",22)/2, H-70, 22, {255,255,100,255});
        }

        // ── Panel del estado del clima (Esquina superior izquierda)
        DrawRectangleRounded({10,10,185,50}, 0.2f, 8, {0,0,0,150});
        DrawText(cfg.nombreClima.c_str(), 22, 16, 18, {120,210,255,255});
        sprintf(buf, "Agarre: %.0f%%", cfg.factorClima*100.0f); // Multiplicador de fricción de las físicas
        DrawText(buf, 22, 38, 13, {180,180,180,200});

        // ── Tabla dinámica de posiciones de la sesión en vivo (Esquina superior derecha)
        int nA = (int)cfg.autos.size();
        int panH = 28 + nA*20; // Escalado dinámico de altura del panel según el número de vehículos en pista
        DrawRectangleRounded({(float)(W-175),10,165,(float)panH}, 0.15f, 8, {0,0,0,160});
        DrawText("POSICIONES", W-168, 16, 13, {255,180,0,255});
        
        // Algoritmo de ordenamiento visual: Busca a los pilotos en orden del 1 al N mediante mapeo lineal
        for (int i=0; i<nA; i++) {
            const Auto* au = nullptr;
            for (auto& a3 : cfg.autos)
                if (a3.posicion == i+1) { au = &a3; break; }
            if (!au) continue;
            
            sprintf(buf, "%d. %s", au->posicion, au->piloto.substr(0,13).c_str());
            Color cl = au->accidentado ? Color{255,80,80,255} : // Texto rojo si chocó
                       au->esJugador   ? Color{255,220,0,255} : WHITE; // Texto amarillo si eres tú
            DrawText(buf, W-168, 32+i*20, 13, cl);
        }

        // ── Barra inferior informativa de controles y cámaras
        const char* vs = vista==VistaCamera::TERCERA_PERSONA ? "3a PERSONA" :
                         vista==VistaCamera::AEREA            ? "AEREA"      :
                         vista==VistaCamera::TRANSMISION      ? "TV"         : "LIBRE";
        sprintf(buf, "[V] %s  |  [<>] %s", vs, cfg.autos[camTarget].piloto.substr(0,12).c_str());
        DrawRectangle(0, H-30, W, 30, {0,0,0,120});
        DrawText(buf, 10, H-22, 13, {160,160,160,200});
        DrawText("[↑↓] Acelerar/Frenar   [ESC] Pausa", W-260, H-22, 13, {160,160,160,200});
    }

    // ────────────────────────────────────────────────────────
    //  MENÚ OVERLAY (Paneles de pausa / Opciones principales)
    //  Explicación para tu exposición: Dibuja un menú contextual 2D centrado de forma matemática 
    //  en el eje de coordenadas de la ventana, calculando dinámicamente las dimensiones según las 
    //  cadenas de texto guardadas en el vector `opciones` e iluminando la opción `seleccionado`.
    // ────────────────────────────────────────────────────────
    static void DrawMenuOverlay(const char* titulo,
                                const std::vector<std::string>& opciones,
                                int seleccionado)
    {
        int W = GetScreenWidth();
        int H = GetScreenHeight();

        // Bandera lógica para comprobar si el menú posee un título superior
        bool tieneTitulo = titulo && titulo[0] != '\0';

        // Dimensiones proporcionales del cuadro del menú
        int panW = 600;
        int panH = (tieneTitulo ? 120 : 70) + (int)opciones.size() * 65;
        int panX = W / 2 - panW / 2; // Centrado horizontal
        int panY = tieneTitulo ? 120 : 245;

        // Fondo y bordes estilizados del panel con degradados redondeados
        DrawRectangleRounded({(float)panX,(float)panY,(float)panW,(float)panH}, 0.08f, 12, {8,8,18,230});
        DrawRectangleRoundedLines({(float)panX,(float)panY,(float)panW,(float)panH}, 0.08f, 12, {255,180,0,120});

        int optionStartY;

        // Si incluye título, calcula su longitud en píxeles y dibuja una línea divisoria decorativa
        if (tieneTitulo) {
            int tw = MeasureText(titulo, 38);
            DrawText(titulo, W/2-tw/2, panY+22, 38, {255,180,0,255});
            DrawRectangle(W/2-220, panY+68, 440, 2, {255,180,0,160});
            optionStartY = panY + 86;
        } else {
            optionStartY = panY + 26;
        }

        // Bucle iterador para imprimir las opciones disponibles en el vector
        for (int i=0; i<(int)opciones.size(); i++) {
            bool sel = (i==seleccionado); // Booleano que indica si el elemento es el seleccionado actualmente
            int y = optionStartY + i*62;  // Separación vertical uniforme entre filas de opciones

            // Si está seleccionado, dibuja un marco de realce amarillo iluminado en su contenedor trasero
            if (sel) {
                DrawRectangleRounded({(float)(W/2-220),(float)(y-6),440,48}, 0.25f, 8, {255,180,0,35});
                DrawRectangleRoundedLines({(float)(W/2-220),(float)(y-6),440,48}, 0.25f, 8, {255,180,0,180});
            }

            int sz = sel ? 26 : 20; // Aumento de tamaño de fuente tipográfica dinámico al enfocar
            int tw2 = MeasureText(opciones[i].c_str(), sz);
            Color cl = sel ? WHITE : Color{170,170,170,200}; // Atenuación de color para los ítems inactivos
            DrawText(opciones[i].c_str(), W/2-tw2/2, y+4, sz, cl);
        }

        // Leyenda de controles del teclado al pie de la pantalla
        DrawText("[ ENTER ] Seleccionar   [ ESC ] Atras",
                 W/2-MeasureText("[ ENTER ] Seleccionar   [ ESC ] Atras",13)/2, H-35, 13, {120,120,120,180});
    }
};
