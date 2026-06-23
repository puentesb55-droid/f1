#define _USE_MATH_DEFINES
#include <cmath>
#include "raylib.h"    // Gráficos y ventanas
#include "raymath.h"   // Matemáticas de vectores 3D
#include "f1_types.h"  // Nuestras estructuras de datos fijas
#include "track.h"     // Generación de pistas
#include "physics.h"   // Motor físico de la IA
#include "renderer.h"  // Funciones de dibujo procedural 3D
#include "config.h"    // Guardar y leer archivos de texto de configuración
#include <vector>
#include <string>
#include <algorithm>   // Necesario para usar std::sort
#include <cstdio>
#include <ctime>       // Para inicializar los números aleatorios con el reloj

// ═══════════════════════════════════════════════════════════
//  CÁLCULOS DE CÁMARAS INTEGRADOS
// ═══════════════════════════════════════════════════════════

// Cámara en 3ra Persona: Sigue al coche desde atrás de forma dinámica
Camera3D Cam3P(const Auto& a) {
    Camera3D cam = {};
    // La distancia hacia atrás aumenta ligeramente con la velocidad (efecto de velocidad)
    float behind = 10.0f + a.velocidadActual * 0.015f;
    
    // Posición de la cámara calculada usando el ángulo 'yaw' (dirección del auto)
    cam.position = { a.pos3D.x - sinf(a.yaw)*behind, a.pos3D.y+4.0f, a.pos3D.z - cosf(a.yaw)*behind };
    cam.target   = { a.pos3D.x, a.pos3D.y+0.8f, a.pos3D.z }; // Hacia dónde mira la cámara
    cam.up={0,1,0}; // Eje Y apunta hacia arriba
    cam.fovy=72.0f+a.velocidadActual*0.025f; // El campo de visión (FOV) se abre al ir rápido
    cam.projection=CAMERA_PERSPECTIVE;
    return cam;
}

float zoomAereo = 90.0f;
// Cámara Aérea: Enfoca al grupo de autos promediando sus posiciones
Camera3D CamAerea(const std::vector<Auto>& autos) {
    Vector3 c={0,0,0};
    for (auto& a:autos) c=Vector3Add(c,a.pos3D); // Suma las posiciones de todos los autos
    c=Vector3Scale(c,1.0f/autos.size());        // Saca el centro geométrico (promedio)

    // Captura el movimiento de la rueda del mouse para ajustar el zoom
    float wheel = GetMouseWheelMove();
    zoomAereo -= wheel * 15.0f;
    zoomAereo = std::clamp(zoomAereo, 30.0f, 600.0f);  // Restringe el zoom mínimo y máximo

    Camera3D cam={};
    cam.position={c.x, c.y+zoomAereo, c.z+20.0f}; // Situada en el cielo apuntando al centro
    cam.target=c;
    cam.up={0,1,0}; cam.fovy=55.0f; cam.projection=CAMERA_PERSPECTIVE;
    return cam;
}

// Cámara de Transmisión (TV): Estilo televisión desde un costado de la curva
Camera3D CamTV(const Auto& a) {
    Camera3D cam={};
    // Se posiciona usando funciones trigonométricas fijas respecto al auto
    cam.position={a.pos3D.x+cosf(a.yaw)*18.0f,a.pos3D.y+5.5f,a.pos3D.z-sinf(a.yaw)*18.0f};
    cam.target={a.pos3D.x,a.pos3D.y+0.5f,a.pos3D.z};
    cam.up={0,1,0}; cam.fovy=50.0f; cam.projection=CAMERA_PERSPECTIVE;
    return cam;
}

// ═══════════════════════════════════════════════════════════
//  SISTEMA DE POSICIONAMIENTO EN TIEMPO REAL (ALGORITMO)
// ═══════════════════════════════════════════════════════════
void OrdenarPos(std::vector<Auto>& autos) {
    std::vector<int> idx(autos.size());
    for (int i=0;i<(int)idx.size();i++) idx[i]=i; // Llena el índice con 0,1,2...

    // std::sort con función Lambda: compara los coches para ver quién va ganando
    std::sort(idx.begin(),idx.end(),[&](int a,int b){
        if(autos[a].vuelta!=autos[b].vuelta) return autos[a].vuelta>autos[b].vuelta; // Prioridad 1: Más vueltas
        return autos[a].distanciaRecorrida>autos[b].distanciaRecorrida; // Prioridad 2: Más distancia en la vuelta actual
    });
    
    // Le asigna el número de posición real (1° al 8°) a la estructura del coche
    for (int i=0; i<(int)idx.size(); i++) autos[idx[i]].posicion=i+1;
}

// Verifica si algún auto ya completó las vueltas pactadas sin chocar
int CheckGanador(const std::vector<Auto>& autos,int vueltas){
    for(int i=0;i<(int)autos.size();i++)
        if(autos[i].vuelta>=vueltas&&!autos[i].accidentado) return i; // Devuelve el ID del ganador
    return -1; // Nadie ha ganado aún
}

// ═══════════════════════════════════════════════════════════
//  FUNCIÓN PRINCIPAL (MAIN)
// ═══════════════════════════════════════════════════════════
int main() {
    srand((unsigned)time(nullptr)); // Semilla aleatoria basada en el tiempo para que cada carrera sea distinta
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);  // Permite al usuario estirar la ventana
    InitWindow(SCREEN_W,SCREEN_H,"F1 Simulator 3D"); // Inicializa la ventana gráfica
    MaximizeWindow();                        // Abre el juego en pantalla completa nativa
    SetTargetFPS(60);                        // Sincroniza el juego a 60 cuadros por segundo
    DisableCursor();                         // Oculta el puntero del mouse dentro del juego

    // Inicialización de Variables de Estado
    GameState state   =GameState::MENU_PRINCIPAL;
    VistaCamera  camVista=VistaCamera::TERCERA_PERSONA;
    int camTarget=0,menuSel=0,pistaSel=0,climaSel=0,vueltasSel=1,pilotoSel=0;
    bool carreraFin=false; int ganadorIdx=-1; float tiempoRes=0;
    float countdown=0; bool carreraActiva=false;
    int filaActiva=0; // Controla qué sección del menú de pilotos está activa

    ConfigCarrera cfg=Config::Leer("carrera_config.txt"); // Lee la última configuración del disco duro
    std::vector<NodoPista> nodos; // Buffer dinámico donde se cargará la pista actual
    Camera3D cam={};
    cam.position={0,20,40};cam.target={0,0,0};cam.up={0,1,0};cam.fovy=60;cam.projection=CAMERA_PERSPECTIVE;
    Camera3D camLibre=cam; // Duplicado de cámara para el modo espectador libre

    // Expresión Lambda para reiniciar los datos de los coches al cambiar de circuito
    auto RecargarPista=[&](){
        switch(cfg.pista){
            case PistaID::MONACO:      nodos=PistaBuilder::BuildMonaco();      break;
            case PistaID::SILVERSTONE: nodos=PistaBuilder::BuildSilverstone(); break;
            case PistaID::SUZUKA:      nodos=PistaBuilder::BuildSuzuka();      break;
        }
        // Acomoda los autos en fila india (Grid de salida) espaciados por 6 metros
        for(int i=0;i<(int)cfg.autos.size();i++){
            cfg.autos[i].distanciaRecorrida=(float)(i*6);
            cfg.autos[i].vuelta=0;cfg.autos[i].tiempoVuelta=0;cfg.autos[i].tiempoTotal=0;
            cfg.autos[i].mejorVuelta=0;cfg.autos[i].velocidadActual=0;
            cfg.autos[i].derrapeIntensidad=0;cfg.autos[i].accidentado=false;
            cfg.autos[i].turnosParado=0;cfg.autos[i].posicion=i+1;
            
            // Posiciona físicamente el auto sobre las coordenadas 3D del nodo de la pista
            int idx2=(int)cfg.autos[i].distanciaRecorrida%(int)nodos.size();
            cfg.autos[i].pos3D=nodos[idx2].centro; cfg.autos[i].pos3D.y+=0.5f;
            // Calcula el ángulo de rotación inicial usando la tangente del camino
            cfg.autos[i].yaw=atan2f(nodos[idx2].tangente.x,nodos[idx2].tangente.z);
            cfg.autos[i].rastroIzq.clear(); cfg.autos[i].rastroDer.clear(); // Limpia marcas viejas de llantas
        }
        carreraFin=false;ganadorIdx=-1;tiempoRes=0;
    };
    RecargarPista(); // Ejecuta la recarga al arrancar el programa

    // ────────────────────────────────────────────────────────
    //  BUCLE PRINCIPAL DEL JUEGO
    // ────────────────────────────────────────────────────────
    while(!WindowShouldClose()){
        // dt (DeltaTime): Tiempo real que pasó entre el cuadro anterior y el actual. Evita desfases de velocidad si el juego se traba
        float dt=std::min(GetFrameTime(),0.033f);

        // ─── SECCIÓN DE LOGICA (MÁQUINA DE ESTADOS) ───
        if(state==GameState::MENU_PRINCIPAL){
            EnableCursor(); // Muestra el mouse en menús
            if(IsKeyPressed(KEY_DOWN)) menuSel=(menuSel+1)%3;
            if(IsKeyPressed(KEY_UP))   menuSel=(menuSel+2)%3;
            if(IsKeyPressed(KEY_ENTER)){
                if(menuSel==0){state=GameState::SELECCION_PISTA;menuSel=0;}
                if(menuSel==1){cfg=Config::Leer("carrera_config.txt");RecargarPista();countdown=3;carreraActiva=false;state=GameState::CARRERA;DisableCursor();}
                if(menuSel==2) break; // Cierra el programa
            }
        }
        else if(state==GameState::SELECCION_PISTA){
            if(IsKeyPressed(KEY_DOWN)) pistaSel=(pistaSel+1)%3;
            if(IsKeyPressed(KEY_UP))   pistaSel=(pistaSel+2)%3;
            if(IsKeyPressed(KEY_ENTER)){cfg.pista=(PistaID)pistaSel;state=GameState::SELECCION_PILOTOS;}
            if(IsKeyPressed(KEY_ESCAPE)){state=GameState::MENU_PRINCIPAL;menuSel=0;}
        }
        else if(state==GameState::SELECCION_PILOTOS){
            // Te permite saltar entre la fila de Pilotos, la de Clima y la de Vueltas usando TAB
            if(IsKeyPressed(KEY_TAB)) filaActiva=(filaActiva+1)%3;
            if(IsKeyPressed(KEY_UP)){
                if(filaActiva==0) pilotoSel=(pilotoSel+7)%8;
                else if(filaActiva==1) climaSel=(climaSel+3)%4;
            }
            if(IsKeyPressed(KEY_DOWN)){
                if(filaActiva==0) pilotoSel=(pilotoSel+1)%8;
                else if(filaActiva==1) climaSel=(climaSel+1)%4;
            }
            if(IsKeyPressed(KEY_LEFT)){
                if(filaActiva==2&&vueltasSel>0) vueltasSel--;
                else if(filaActiva==0) pilotoSel=(pilotoSel+7)%8;
            }
            if(IsKeyPressed(KEY_RIGHT)){
                if(filaActiva==2&&vueltasSel<4) vueltasSel++;
                else if(filaActiva==0) pilotoSel=(pilotoSel+1)%8;
            }
            if(IsKeyPressed(KEY_ENTER)){
                // Guarda las selecciones hechas por el usuario en la configuración global
                const char* cs[]={"Soleado","Nublado","Lluvia","Tormenta"};
                cfg.clima=Config::ClimaEnum(cs[climaSel]);
                cfg.factorClima=Config::ClimaTofactor(cs[climaSel]);
                cfg.nombreClima=cs[climaSel]; cfg.nombreClima[0]=toupper(cfg.nombreClima[0]);
                cfg.vueltas=3+vueltasSel*2;
                
                // Busca qué auto seleccionó el humano y le activa la bandera '.esJugador'
                for(auto& a:cfg.autos) a.esJugador=false;
                bool found=false;
                for(auto& a:cfg.autos){if(CATALOGO_PILOTOS[pilotoSel].nombre==a.piloto){a.esJugador=true;found=true;break;}}
                if(!found) cfg.autos[0].esJugador=true;
                
                Config::Guardar(cfg,"carrera_config.txt"); // Guarda los datos en un archivo físico
                RecargarPista();
                countdown=3; carreraActiva=false; // Inicia el conteo regresivo (3, 2, 1... ¡YA!)
                camTarget=0;
                for(int i=0;i<(int)cfg.autos.size();i++) if(cfg.autos[i].esJugador){camTarget=i;break;}
                state=GameState::CARRERA; DisableCursor(); filaActiva=0;
            }
            if(IsKeyPressed(KEY_ESCAPE)) state=GameState::SELECCION_PISTA;
        }
        else if(state==GameState::CARRERA){
            // Controles de cámara durante la carrera
            if(IsKeyPressed(KEY_V)){camVista=(VistaCamera)(((int)camVista+1)%4);if(camVista==VistaCamera::LIBRE)EnableCursor();else DisableCursor();}
            if(IsKeyPressed(KEY_PERIOD)) camTarget=(camTarget+1)%(int)cfg.autos.size(); // Alterna auto enfocado con '.'
            if(IsKeyPressed(KEY_COMMA))  camTarget=(camTarget+(int)cfg.autos.size()-1)%(int)cfg.autos.size(); // Alterna con ','
            if(IsKeyPressed(KEY_ESCAPE)){state=GameState::PAUSA;EnableCursor();}
            
            // Manejo del semáforo de salida
            if(countdown>0){countdown-=dt;if(countdown<=0){countdown=0;carreraActiva=true;}}
            
            if(carreraActiva&&!carreraFin){
                // 🏎️ MOTOR FÍSICO DEL JUGADOR REALISTA
                for (auto& pl : cfg.autos) {
                    if (!pl.esJugador || pl.accidentado) continue;
                    
                    bool ac = IsKeyDown(KEY_UP) || IsKeyDown(KEY_W);   // Acelerar
                    bool br = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S); // Frenar
                    int ni = Fisica::GetNodeIndex(pl, nodos);          // Saber en qué parte de la pista está
                    float limV = nodos[ni].limitVelocidad * cfg.factorClima; // Velocidad límite recomendada ajustada por lluvia
                    
                    // Fórmulas físicas de aceleración y resistencia exponencial
                    if (ac) pl.velocidadActual += pl.aceleracion * dt * 3.6f * pl.agarre;
                    else if (br) pl.velocidadActual -= pl.frenado * dt * 3.6f * 1.8f;
                    else pl.velocidadActual -= pl.frenado * dt * 0.4f; // Fricción natural del viento
                    
                    pl.velocidadActual = std::max(0.0f, std::min(pl.velocidadActual, pl.velocidadBase));
                    
                    // Cálculos matemáticos del derrape por exceso de velocidad en curvas cerradas
                    float cs2 = (nodos[ni].esCurva && nodos[ni].radio < 12.0f) ? std::max(0.0f, 1.0f - nodos[ni].radio / 12.0f) : 0.0f;
                    float exc = (pl.velocidadActual / std::max(limV, 10.0f)) - 1.08f;
                    if (exc > 0 && cs2 > 0.4f) pl.derrapeIntensidad += exc * cs2 * dt * 0.6f;
                    else pl.derrapeIntensidad -= dt * 2.5f * pl.destreza;
                    
                    pl.derrapeIntensidad = std::max(0.0f, std::min(1.0f, pl.derrapeIntensidad));
                    
                    // Factor de riesgo: Si derrapas al extremo (>96%), hay probabilidad aleatoria de chocar
                    if (pl.derrapeIntensidad > 0.96f && (float)rand()/RAND_MAX < 0.003f) {
                        pl.accidentado = true; pl.turnosParado = 0; pl.derrapeIntensidad = 0;
                    }
                    
                    // Convierte la velocidad de Km/h a m/s y avanza el coche sobre la pista
                    float dist = (pl.velocidadActual / 3.6f) * dt;
                    pl.distanciaRecorrida += dist;
                    float pLen = (float)nodos.size();
                    
                    // Al pasar por la meta (cuando la distancia supera el total de la pista) sube una vuelta
                    if (pl.distanciaRecorrida >= pLen) {
                        pl.distanciaRecorrida -= pLen; pl.vuelta++;
                        pl.mejorVuelta = (pl.mejorVuelta < 0.1f) ? pl.tiempoVuelta : std::min(pl.mejorVuelta, pl.tiempoVuelta);
                        pl.tiempoVuelta = 0;
                    }
                    pl.tiempoVuelta += dt; pl.tiempoTotal += dt;
                    Fisica::UpdateTransform3D(pl, nodos, dt); // Actualiza matrices de rotación espacial 3D
                }

                // 🤖 ACTUALIZACIÓN AUTOMÁTICA DE LA INTELIGENCIA ARTIFICIAL (IA)
                for (auto& a : cfg.autos) {
                    if (!a.esJugador)
                        Fisica::UpdateIA(a, nodos, cfg.autos, cfg.factorClima, dt);
                }
                
                // Sistema de recuperación automática tras un accidente (penalización de 3 segundos / 180 cuadros)
                for(auto& a:cfg.autos){
                    if(!a.accidentado) continue;
                    a.turnosParado++;
                    if(a.turnosParado>180){a.accidentado=false;a.turnosParado=0;a.velocidadActual=20;}
                }
                
                OrdenarPos(cfg.autos); // Recalcula las posiciones (1° al 8°)
                ganadorIdx=CheckGanador(cfg.autos,cfg.vueltas); // Verifica si alguien ya cruzó la bandera a cuadros
                if(ganadorIdx>=0) carreraFin=true;
            }
            if(carreraFin){tiempoRes+=dt;if(tiempoRes>2&&IsKeyPressed(KEY_ENTER)) state=GameState::RESULTADOS;}
            
            // Asigna los parámetros matemáticos a la cámara según la vista activa
            switch(camVista){
                case VistaCamera::TERCERA_PERSONA: cam=Cam3P(cfg.autos[camTarget]); break;
                case VistaCamera::AEREA:           cam=CamAerea(cfg.autos); break;
                case VistaCamera::TRANSMISION:     cam=CamTV(cfg.autos[camTarget]); break;
                case VistaCamera::LIBRE:
                    // Cámara libre controlada por mouse y las teclas W, A, S, D
                    UpdateCameraPro(&camLibre,
                        {(IsKeyDown(KEY_W)-IsKeyDown(KEY_S))*25.0f*dt,(IsKeyDown(KEY_D)-IsKeyDown(KEY_A))*25.0f*dt,0},
                        {GetMouseDelta().x*0.1f,GetMouseDelta().y*0.1f,0},0);
                    cam=camLibre; break;
            }
        }
        else if(state==GameState::PAUSA){
            if(IsKeyPressed(KEY_ESCAPE)||IsKeyPressed(KEY_P)){state=GameState::CARRERA;DisableCursor();}
            if(IsKeyPressed(KEY_M)){state=GameState::MENU_PRINCIPAL;menuSel=0;}
        }
        else if(state==GameState::RESULTADOS){
            if(IsKeyPressed(KEY_ENTER)||IsKeyPressed(KEY_ESCAPE)){state=GameState::MENU_PRINCIPAL;menuSel=0;}
        }

        // ─── SECCIÓN DE RENDER (DIBUJO EN PANTALLA) ───
        BeginDrawing();
            int SW = GetScreenWidth();
            int SH = GetScreenHeight();

            Renderer::DrawSkybox(cfg.clima); // Dibuja el cielo dinámico (Lluvia = gris, Soleado = azul)

            if(state==GameState::CARRERA||state==GameState::PAUSA||state==GameState::RESULTADOS){
                BeginMode3D(cam); // !!! TODO LO QUE ESTÉ AQUÍ ADENTRO SE DIBUJA EN ESPACIO TRIDIMENSIONAL 3D !!!
                    PistaRenderer::DrawPaisaje(nodos,cfg.pista,cfg.clima); // Terreno exterior
                    PistaRenderer::Draw(nodos,cfg.pista);                  // Asfalto y pianitos triangulados
                    PistaRenderer::DrawMeta(nodos);                       // Línea de meta cuadriculada
                    Renderer::DrawRain(cam,cfg.clima,GetTime());          // Sistema de partículas de lluvia
                    for(auto& a:cfg.autos){Renderer::DrawTireMarks(a); Renderer::DrawCarF1(a);} // Marcas de llantas y autos F1
                EndMode3D(); // Termina el espacio 3D. Lo siguiente es interfaz plana 2D (Marcadores, textos)

                // Dibuja los nombres flotantes sobre cada coche (Cálculo matemático WorldToScreen)
                for(auto& a:cfg.autos){
                    Vector2 sp=GetWorldToScreen(Vector3Add(a.pos3D,{0,3.2f,0}),cam); // Pone la etiqueta 3.2 metros arriba del coche
                    if(sp.x>10&&sp.x<SW-10&&sp.y>10&&sp.y<SH-50){
                        const char* nm=a.piloto.substr(0,13).c_str();
                        int tw=MeasureText(nm,11);
                        DrawRectangle((int)sp.x-tw/2-3,(int)sp.y-1,tw+6,14,{0,0,0,150}); // Fondo negro semitransparente
                        DrawText(nm,(int)sp.x-tw/2,(int)sp.y,11,a.esJugador?YELLOW:WHITE);
                    }
                }
                
                if(state==GameState::CARRERA){
                    Auto* pp=nullptr; for(auto& a:cfg.autos) if(a.esJugador){pp=&a;break;}
                    if(pp) Renderer::DrawHUD(cfg,*pp,camVista,camTarget,pp->tiempoTotal); // Interfaz de telemetría, velocímetro y tabla de tiempos
                    
                    // Renderiza los números gigantes del conteo de salida
                    if(!carreraActiva||countdown>0){
                        int cnt=(int)ceil(countdown);
                        char cb[8]; if(cnt>0)sprintf(cb,"%d",cnt);else sprintf(cb,"GO!");
                        int tw=MeasureText(cb,90);
                        Color cc=cnt>0?Color{255,60,60,255}:Color{60,255,60,255};
                        DrawRectangle(SW/2-tw/2-20,SH/2-55,tw+40,110,{0,0,0,180});
                        DrawText(cb,SW/2-tw/2,SH/2-50,90,cc);
                    }
                    // Letrero flotante de fin de carrera
                    if(carreraFin&&ganadorIdx>=0){
                        char buf[64]; sprintf(buf,"GANADOR: %s",cfg.autos[ganadorIdx].piloto.c_str());
                        int tw=MeasureText(buf,32);
                        DrawRectangle(SW/2-tw/2-20,SH/2-26,tw+40,62,{0,0,0,210});
                        DrawText(buf,SW/2-tw/2,SH/2-16,32,{255,220,0,255});
                        DrawText("[ ENTER ] Ver resultados",SW/2-MeasureText("[ ENTER ] Ver resultados",14)/2,SH/2+42,14,WHITE);
                    }
                }
                if(state==GameState::PAUSA){
                    DrawRectangle(0,0,SW,SH,{0,0,0,170});
                    DrawText("PAUSA",SW/2-MeasureText("PAUSA",56)/2,SH/2-60,56,{255,180,0,255});
                    DrawText("[ P / ESC ] Continuar",SW/2-MeasureText("[ P / ESC ] Continuar",20)/2,SH/2+20,20,WHITE);
                    DrawText("[ M ] Menu principal",SW/2-MeasureText("[ M ] Menu principal",20)/2,SH/2+52,20,WHITE);
                }
                if(state==GameState::RESULTADOS){
                    // Dibuja el podio final calculando minutos, segundos y milisegundos con fmodf
                    DrawRectangle(0,0,SW,SH,{0,0,0,210});
                    DrawText("RESULTADOS FINALES",SW/2-MeasureText("RESULTADOS FINALES",36)/2,50,36,{255,180,0,255});
                    auto sorted=cfg.autos;
                    std::sort(sorted.begin(),sorted.end(),[](const Auto&a,const Auto&b){return a.posicion<b.posicion;});
                    for(int i=0;i<(int)sorted.size();i++){
                        char buf[80];
                        int mn=(int)(sorted[i].tiempoTotal/60),sc=(int)fmodf(sorted[i].tiempoTotal,60);
                        int ms=(int)((sorted[i].tiempoTotal-floorf(sorted[i].tiempoTotal))*1000);
                        sprintf(buf,"%d.  %-22s  %d:%02d.%03d  V%d",i+1,sorted[i].piloto.c_str(),mn,sc,ms,sorted[i].vuelta);
                        Color cl=i==0?Color{255,220,0,255}:i<3?Color{180,220,255,255}:WHITE; // Color Oro, Plata o Blanco según podio
                        DrawText(buf,SW/2-MeasureText(buf,18)/2,130+i*42,18,cl);
                    }
                    DrawText("[ ENTER ] Menu",SW/2-MeasureText("[ ENTER ] Menu",14)/2,SH-50,14,{180,180,180,200});
                }
            }

            // ─── RENDER DE MENÚS (INTERFAZ 2D PLANA) ───
            if(state==GameState::MENU_PRINCIPAL){
                DrawRectangle(0,0,SW,SH,{6,6,16,255});
                DrawText("F1",SW/2-MeasureText("F1",90)/2,40,90,{220,30,30,255});
                DrawText("SIMULATOR 3D",SW/2-MeasureText("SIMULATOR 3D",26)/2,136,26,{200,200,200,220});
                Renderer::DrawMenuOverlay("",{"Nueva Carrera","Cargar Config","Salir"},menuSel);
            }
            else if(state==GameState::SELECCION_PISTA){
                DrawRectangle(0,0,SW,SH,{6,6,16,255});
                Renderer::DrawMenuOverlay("SELECCIONAR PISTA",
                    {"Monaco   Callejero, curvas muy cerradas",
                    "Silverstone    Alta velocidad, curvas rapidas",
                    "Suzuka    Tecnica, curvas S y 130R"},pistaSel);
            }
            else if(state==GameState::SELECCION_PILOTOS){
                // Interfaz de selección avanzada de piloto, clima y vueltas usando bloques procedurales interactivos
                int W2=GetScreenWidth(), H2=GetScreenHeight();
                DrawRectangle(0,0,W2,H2,{6,6,16,255});
                DrawText("CONFIGURAR CARRERA",W2/2-MeasureText("CONFIGURAR CARRERA",30)/2,28,30,{255,180,0,255});
                DrawRectangle(W2/2-280,66,560,2,{255,180,0,140});

                bool fP=(filaActiva==0),fC=(filaActiva==1),fV=(filaActiva==2);
                
                // Muestra la tarjeta del Piloto con sus estadísticas de velocidad procedentes del catálogo
                if(fP) DrawRectangle(W2/2-285,76,570,158,{255,180,0,15});
                DrawRectangleLines(W2/2-285,76,570,158,fP?Color{255,180,0,120}:Color{60,60,60,120});
                DrawText("PILOTO JUGADOR",W2/2-270,82,14,fP?Color{255,220,0,255}:Color{140,140,140,200});
                DrawText(fP?"[ UP/DOWN ] Cambiar":"",W2/2+50,82,12,{120,120,120,180});
                for(int d=-1;d<=1;d++){
                    int idx2=(pilotoSel+8+d)%8;
                    const auto& dp=CATALOGO_PILOTOS[idx2];
                    bool sel=(d==0); int x=W2/2+d*220;
                    if(sel){DrawRectangleRounded({(float)(x-105),100,210,120},0.2f,8,{255,180,0,30});}
                    DrawRectangle(x-40,106,80,7,dp.colorCuerpo);
                    DrawText(dp.nombre.c_str(),x-MeasureText(dp.nombre.c_str(),sel?17:12)/2,118,sel?17:12,sel?WHITE:Color{120,120,120,180});
                    DrawText(dp.equipo.substr(0,18).c_str(),x-MeasureText(dp.equipo.substr(0,18).c_str(),11)/2,142,11,sel?Color{180,180,180,220}:Color{90,90,90,160});
                    if(sel){char sb[32];sprintf(sb,"%.0f km/h  Dest %.0f%%",dp.velBase,dp.destreza*100);DrawText(sb,x-MeasureText(sb,11)/2,162,11,{150,220,150,220});}
                }
                DrawText("<",W2/2-268,148,22,fP?Color{255,180,0,255}:Color{60,60,60,180});
                DrawText(">",W2/2+258,148,22,fP?Color{255,180,0,255}:Color{60,60,60,180});

                // Sección Clima
                if(fC) DrawRectangle(W2/2-285,242,570,88,{255,180,0,15});
                DrawRectangleLines(W2/2-285,242,570,88,fC?Color{255,180,0,120}:Color{60,60,60,120});
                DrawText("CLIMA",W2/2-270,248,14,fC?Color{255,220,0,255}:Color{140,140,140,200});
                DrawText(fC?"[ UP/DOWN ] Cambiar":"",W2/2+50,248,12,{120,120,120,180});
                const char* cn[]={"Soleado","Nublado","Lluvia","Tormenta"};
                const char* ag[]={"100%","88%","72%","52%"}; // Indica el agarre disponible en pantalla
                for(int i=0;i<4;i++){
                    bool s=(i==climaSel); int x=W2/2-210+i*140;
                    if(s) DrawRectangleRounded({(float)(x-12),268,110,48},0.25f,8,{255,180,0,35});
                    DrawText(cn[i],x,274,s?18:13,s?Color{255,220,0,255}:Color{120,120,120,180});
                    DrawText(ag[i],x+4,298,12,s?Color{150,255,150,255}:Color{80,80,80,160});
                }

                // Sección Laps (Vueltas)
                if(fV) DrawRectangle(W2/2-285,338,570,78,{255,180,0,15});
                DrawRectangleLines(W2/2-285,338,570,78,fV?Color{255,180,0,120}:Color{60,60,60,120});
                DrawText("VUELTAS",W2/2-270,344,14,fV?Color{255,220,0,255}:Color{140,140,140,200});
                DrawText(fV?"[ LEFT/RIGHT ] Cambiar":"",W2/2+50,344,12,{120,120,120,180});
                int vopts[]={3,5,7,9,11};
                for(int i=0;i<5;i++){
                    bool s=(i==vueltasSel); int x=W2/2-180+i*90;
                    if(s) DrawRectangleRounded({(float)(x-16),364,46,40},0.3f,8,{255,180,0,35});
                    char b[4]; sprintf(b,"%d",vopts[i]);
                    DrawText(b,x,370,s?28:20,s?Color{255,220,0,255}:Color{120,120,120,180});
                }

                // Cuadro inferior del Grid de Salida estructurado dinámicamente
                DrawRectangle(W2/2-285,424,570,178,{0,0,0,60});
                DrawText("GRID DE SALIDA:",W2/2-270,430,13,{160,160,160,200});
                for(int i=0;i<(int)cfg.autos.size()&&i<8;i++){
                    bool esJ=(CATALOGO_PILOTOS[pilotoSel].nombre==cfg.autos[i].piloto);
                    char b[64]; sprintf(b,"%d. %s — %s%s",i+1,cfg.autos[i].piloto.c_str(),cfg.autos[i].equipo.substr(0,18).c_str(),esJ?" ★ JUGADOR":"");
                    DrawText(b,W2/2-270,448+i*19,13,esJ?YELLOW:WHITE);
                }

                DrawText("[ TAB ] Cambiar seccion   [ ENTER ] Iniciar   [ ESC ] Atras",
                    W2/2-MeasureText("[ TAB ] Cambiar seccion   [ ENTER ] Iniciar   [ ESC ] Atras",13)/2,H2-30,13,{120,120,120,180});
            }

            DrawFPS(12,96); // Muestra los fotogramas por segundo del rendimiento de Raylib
        EndDrawing();
    }
    
    CloseWindow(); // Destruye el contexto gráfico y libera la memoria de la GPU de forma segura
    return 0;      // Finalización exitosa del programa
}    
