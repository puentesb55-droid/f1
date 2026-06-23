#pragma once
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include "f1_types.h"
#include "track.h"

// ═══════════════════════════════════════════════════════════
//  CLASE FÍSICA (SISTEMA DE DINÁMICA, IA Y CINEMÁTICA 3D)
//  Explicación general: Esta clase procesa de forma matemática el modelo de 
//  comportamiento y toma de decisiones de la Inteligencia Artificial (IA), la 
//  simulación estocástica del derrape y accidentes, la cinemática adaptativa de
//  movimiento mediante interpolación lineal (LERP), y la generación de marcas de llantas.
// ═══════════════════════════════════════════════════════════
class Fisica {
public:

    // ── Jugador: llamado desde main.cpp directamente ──────────
    // La lógica del jugador se procesa de forma síncrona en el bucle principal de main.cpp para mayor control de entrada.

    // ────────────────────────────────────────────────────────
    //  ACTUALIZACIÓN LOGICIAL DE LA IA
    //  Explicación para tu exposición: Es la máquina de estados y control físico de los vehículos oponentes.
    //  Implementa un algoritmo predictivo de frenado y trazado basado en la curvatura geométrica de la pista.
    // ────────────────────────────────────────────────────────
    static void UpdateIA(Auto& a,
        const std::vector<NodoPista>& nodos,
        const std::vector<Auto>& todos,
        float factorClima, float dt)
    {
        // Regla de exclusión: Si es el auto controlado por el jugador, omitir el script de IA
        if (a.esJugador) return;

        // SISTEMA DE RECUPERACIÓN (RECOVERY EVENT): Si el auto colisionó o volcó, permanece detenido 200 cuadros (frames)
        if (a.accidentado) {
            a.turnosParado++;
            if (a.turnosParado > 200) {
                a.accidentado    = false;
                a.turnosParado   = 0;
                a.velocidadActual = 18.0f; // Velocidad de reincorporación segura a pista
                a.derrapeIntensidad = 0.0f;
            }
            return;
        }

        int n   = (int)nodos.size();
        int ni  = GetNodeIndex(a, nodos); // Índice del segmento de pista actual ocupado por el auto

        // ALGORITMO DE ANTICIPACIÓN GEOMÉTRICA (Lookahead Horizon)
        // La IA "mira hacia adelante" un número dinámico de nodos en función de su nivel de dificultad.
        // A mayor nivel de IA, analiza curvas más lejanas permitiéndole trazar de forma más eficiente.
        int lookahead = 6 + a.nivelIA * 7;  // Rango de exploración: de 6 a 27 nodos hacia adelante
        float curvMax = 0.0f;
        float radMin  = 999.0f;
        for (int k = 0; k < lookahead; k++) {
            const NodoPista& nk = nodos[(ni + k) % n]; // Aritmética modular para manejar la pista circular cerrada
            if (nk.esCurva) {
                curvMax = std::max(curvMax, nk.curvatura); // Registra la severidad de la peor curva próxima
                radMin  = std::min(radMin, nk.radio);     // Registra el radio más cerrado crítico
            }
        }

        // CÁLCULO VECTORIAL DE VELOCIDAD OBJETIVO (Target Speed Model)
        // Modula la velocidad máxima permitida mezclando límites de la pista, coeficientes climáticos y habilidades del piloto.
        float agresividad = 0.72f + a.nivelIA * 0.09f;
        float velObj = nodos[ni].limitVelocidad
                     * factorClima
                     * agresividad
                     * a.agarre; // Atributo intrínseco de rendimiento del neumático del piloto

        // Reducción predictiva de velocidad: si detecta una curva cerrada (< 55 unidades de radio)
        // se reduce de forma inversamente proporcional a la destreza del piloto (pilotos inexpertos frenan de más)
        if (radMin < 55.0f) {
            float frenadoFactor = std::max(0.0f, 1.0f - (radMin / 55.0f));
            velObj -= frenadoFactor * (1.0f - a.destreza) * 22.0f;
        }
        velObj = std::max(30.0f, velObj); // Cota inferior de seguridad (evita que los autos se detengan por completo)

        // CONTROL DE ACELERACIÓN / FRENADO (Ecuaciones de movimiento lineal)
        if (a.velocidadActual < velObj)
            // Aceleración con factor de conversión métrico (* 3.6f) acoplado a la adherencia de la pista
            a.velocidadActual += a.aceleracion * dt * 3.6f * a.agarre * factorClima;
        else
            // Desaceleración o frenado mecánico constante
            a.velocidadActual -= a.frenado * dt * 3.6f * 0.85f;

        // ALGORITMO DE EVASIÓN Y DISTANCIAMIENTO (Anti-collision Raycasting / Proximidad)
        // Compara de manera lineal el histórico unidimensional del recorrido con el de los demás vehículos.
        for (auto& otro : todos) {
            if (&otro == &a || otro.accidentado) continue;
            float dD = otro.distanciaRecorrida - a.distanciaRecorrida;
            // Si hay un auto adelante a menos de 6 unidades de distancia, aplica un freno de emergencia del 12%
            if (dD > 0 && dD < 6.0f)
                a.velocidadActual *= 0.88f;
        }
        // Restricción matemática obligatoria (Clamp) para mantener la velocidad entre límites operativos
        a.velocidadActual = std::clamp(a.velocidadActual, 5.0f, a.velocidadBase);

        // ── SIMULACIÓN DE FÍSICA DE DERRAPE ESTOCÁSTICO ──────────────────
        const NodoPista& nd = nodos[ni];
        float limV = nd.limitVelocidad * factorClima * a.agarre;
        limV = std::max(limV, 12.0f);

        // Determina la severidad del giro en curvas de radio menor a 40 unidades
        float curvSev = (nd.esCurva && nd.radio < 40.0f)
            ? std::max(0.0f, 1.0f - nd.radio / 40.0f)
            : 0.0f;
        // Comprueba si la velocidad actual excede la tolerancia del umbral de derrape (102% del límite físico)
        float exceso = (a.velocidadActual / limV) - 1.02f;

        // Modelo de probabilidad empírica de pérdida de tracción
        float probD = curvSev * std::max(0.0f, exceso)
                    * (1.2f - a.destreza) // A menor destreza, mayor tasa de fallo
                    * (2.0f - factorClima); // Multiplica el riesgo exponencialmente bajo lluvia/tormenta

        // Evaluación aleatoria Monte Carlo para activar/acumular la intensidad del derrape en tiempo real
        if (probD > 0 && (float)rand()/RAND_MAX < probD * dt * 1.5f)
            a.derrapeIntensidad = std::min(1.0f, a.derrapeIntensidad + probD * 0.5f);

        // Amortiguación natural del derrape (Slip Angle Fade): El auto tiende a recuperar tracción según la destreza del piloto
        a.derrapeIntensidad -= dt * 1.4f * a.destreza;
        a.derrapeIntensidad  = std::max(0.0f, a.derrapeIntensidad);

        // EVALUACIÓN DE CRASH / ACCIDENTE CRÍTICO
        // Si el subviraje o sobreviraje excede el 90%, existe riesgo matemático de colisión letal
        if (a.derrapeIntensidad > 0.90f) {
            float ch = (a.derrapeIntensidad - 0.90f) * (1.2f - a.destreza) * 8.0f;
            if ((float)rand()/RAND_MAX < ch * dt) {
                a.accidentado       = true; // Dispara el estado de siniestro total
                a.turnosParado      = 0;
                a.derrapeIntensidad = 0;
                a.velocidadActual   = 0;
                return;
            }
        }

        // ── ACTUALIZACIÓN DE TELEMETRÍA DE TIEMPOS Y RECORRIDOS ──────────────────
        float dist = (a.velocidadActual / 3.6f) * dt; // Conversión matemática de km/h a m/s multiplicada por Delta Time
        a.distanciaRecorrida += dist;
        float pLen = (float)n;
        
        // Control de cruce de línea de meta (Cierre del ciclo circular de la pista)
        if (a.distanciaRecorrida >= pLen) {
            a.distanciaRecorrida -= pLen; // Normaliza la distancia sin perder remanentes decimales
            a.vuelta++;
            // Lógica de registro para mejor vuelta (Personal Best Record)
            a.mejorVuelta = (a.mejorVuelta < 0.1f)
                ? a.tiempoVuelta
                : std::min(a.mejorVuelta, a.tiempoVuelta);
            a.tiempoVuelta = 0.0f; // Reinicio del cronómetro de vuelta parcial
        }
        a.tiempoVuelta += dt;
        a.tiempoTotal  += dt;

        // Llamada síncrona al resolvedor de transformaciones trigonométricas de la escena 3D
        UpdateTransform3D(a, nodos, dt);
    }

    // ────────────────────────────────────────────────────────
    //  INTERPOLACIÓN CINEMÁTICA Y ACTUALIZACIÓN TRIDIMENSIONAL (UpdateTransform3D)
    //  Explicación para tu exposición: Este método calcula la posición, rotación, peralte
    //  y rastro físico del vehículo en el espacio 3D, suavizando la transición entre nodos.
    // ────────────────────────────────────────────────────────
    static void UpdateTransform3D(Auto& a,
        const std::vector<NodoPista>& nodos, float dt)
    {
        int n    = (int)nodos.size();
        int idx0 = (int)a.distanciaRecorrida % n; // Nodo base inicial
        int idx1 = (idx0 + 1) % n;                // Nodo destino consecutivo
        
        // Factor de interpolación 't' normalizado estrictamente entre [0.0f, 1.0f]
        float t  = a.distanciaRecorrida - floorf(a.distanciaRecorrida);
        t        = std::max(0.0f, std::min(t, 1.0f));

        // INTERPOLACIÓN LINEAL DE POSICIÓN (LERP): Suaviza la traslación del centro del chasis
        Vector3 p0 = nodos[idx0].centro;
        Vector3 p1 = nodos[idx1].centro;
        Vector3 centro = Vector3Lerp(p0, p1, t);
        centro.y += 0.45f; // Elevación física constante sobre el asfalto (evita colisiones con el suelo)

        // Interpolación lineal de vectores tangentes para determinar la orientación geométrica correcta
        Vector3 tang0 = nodos[idx0].tangente;
        Vector3 tang1 = nodos[idx1].tangente;
        Vector3 tangente = Vector3Normalize(Vector3Lerp(tang0, tang1, t));

        // CÁLCULO DE YAW (ÁNGULO DE ROTACIÓN VERTICAL)
        // Utiliza la función arcotangente de dos parámetros (atan2f) para obtener la dirección en radianes
        float yawTarget = atan2f(tangente.x, tangente.z);
        float dyaw = yawTarget - a.yaw;
        
        // Normalización angular: Mantiene el delta de yaw siempre en el rango óptimo de [-PI, +PI] para evitar giros bruscos de 360 grados
        while (dyaw >  (float)M_PI) dyaw -= 2*(float)M_PI;
        while (dyaw < -(float)M_PI) dyaw += 2*(float)M_PI;
        
        // Ajuste de tasa de suavizado angular (Smooth Damping Factor)
        float smooth = nodos[idx0].esCurva ? 9.0f : 5.5f; // Reacción de dirección más rápida dentro de curvas
        a.yaw += dyaw * dt * smooth;

        // EFECTO ONDULATORIO DE DERRAPE LATERAL (Fisical Lateral Slip Vibration)
        // Si hay derrape, induce una vibración armónica cosenoidal en el eje lateral para simular pérdida de adherencia
        Vector3 lateral = { cosf(a.yaw), 0.0f, -sinf(a.yaw) };
        float offsetLat = a.derrapeIntensidad * 1.8f
                        * sinf(a.tiempoTotal * 5.0f); // Frecuencia angular de oscilación de la zaga
        a.pos3D = Vector3Add(centro, Vector3Scale(lateral, offsetLat));

        // PERALTE Y CABECEO (Roll & Pitch Control)
        // Transfiere los valores de inclinación de la pista al auto agregando la distorsión inercial del derrape
        a.roll  = nodos[idx0].inclinacion * 0.7f + a.derrapeIntensidad * 0.14f; // Inclinación en curvas
        a.pitch = -a.derrapeIntensidad * 0.06f; // Hundimiento del morro por pérdida de sustentación

        // ── CONTROL DEL ARREGLO HISTÓRICO DE RASTROS DE LLANTAS (Skidmarks FIFO Buffer) ──
        Vector3 lat2 = { cosf(a.yaw), 0.0f, -sinf(a.yaw) };
        if (a.derrapeIntensidad > 0.15f) {
            // Inserta las posiciones de contacto del neumático izquierdo y derecho calculadas simétricamente
            a.rastroIzq.push_back(Vector3Add(a.pos3D, Vector3Scale(lat2,  CAR_WIDTH*0.48f)));
            a.rastroDer.push_back(Vector3Add(a.pos3D, Vector3Scale(lat2, -CAR_WIDTH*0.48f)));
            
            // Limitador del Buffer circular (Estrategia FIFO): Restringe la memoria a 200 segmentos para evitar memory leaks
            const int MAX_R = 200;
            if ((int)a.rastroIzq.size() > MAX_R) { 
                a.rastroIzq.erase(a.rastroIzq.begin()); 
                a.rastroDer.erase(a.rastroDer.begin()); 
            }
        } else {
            // Desvanecimiento paulatino del rastro (Fade-out) eliminando segmentos viejos cuando se recupera el agarre completo
            if (!a.rastroIzq.empty()) { 
                a.rastroIzq.erase(a.rastroIzq.begin()); 
                a.rastroDer.erase(a.rastroDer.begin()); 
            }
        }
    }

    // Método utilitario en línea para obtener de forma segura el índice acotado del nodo correspondiente
    static int GetNodeIndex(const Auto& a, const std::vector<NodoPista>& nodos) {
        int n = (int)nodos.size();
        if (n == 0) return 0;
        int idx = (int)a.distanciaRecorrida % n;
        return (idx < 0) ? 0 : idx;
    }
};
