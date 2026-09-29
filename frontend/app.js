const API_BASE = String(window.ROUTEMIND_API_BASE || window.location.origin).replace(/\/$/, "");

const state = {
  requests: [],
  brigades: [],
  routes: [],
  unassigned: [],
  explanations: new Map(),
  metrics: {},
  baseline: {},
  replanSummary: "",
  selectedBrigadeId: null,
  filter: "all",
  query: "",
  loading: false
};

const $ = (id) => document.getElementById(id);
let routeMap = null;
let routeLayer = null;
let routeBounds = null;
let routeGeometryRequest = 0;
const routeGeometryCache = new Map();

const statusMeta = {
  new: ["Новая", "new"],
  assigned: ["Назначена", "assigned"],
  route: ["В маршруте", "route"],
  in_work: ["В работе", "in_work"],
  done: ["Завершена", "done"],
  unassigned: ["Без бригады", "unassigned"]
};

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function formatTime(value) {
  if (value === null || value === undefined || value === "") return "—";
  if (typeof value === "number") return formatMinuteOfDay(value);
  const match = String(value).match(/T(\d{2}:\d{2})/);
  if (match) return match[1];
  if (/^\d{2}:\d{2}/.test(String(value))) return String(value).slice(0, 5);
  return "—";
}

function formatMinuteOfDay(minutes) {
  const value = Number(minutes);
  if (!Number.isFinite(value)) return "—";
  return `${String(Math.floor(value / 60) % 24).padStart(2, "0")}:${String(Math.round(value % 60)).padStart(2, "0")}`;
}

function formatDuration(minutes) {
  const value = Number(minutes || 0);
  if (!value) return "—";
  const hours = Math.floor(value / 60);
  const rest = Math.round(value % 60);
  return [hours ? `${hours} ч` : "", rest ? `${rest} мин` : ""].filter(Boolean).join(" ");
}

function shortName(name) {
  const parts = String(name || "Бригада").trim().split(/\s+/);
  if (parts[0] === "Бригада" && parts[1]) return parts[1];
  return parts.length > 1 ? parts[0] : name;
}

function pluralizeRequests(count) {
  const lastTwo = count % 100;
  const last = count % 10;
  if (lastTwo >= 11 && lastTwo <= 14) return "заявок";
  if (last === 1) return "заявка";
  if (last >= 2 && last <= 4) return "заявки";
  return "заявок";
}

function translateReason(reason) {
  const translations = {
    "Missing geolocation": "Нет координат для построения маршрута",
    "No feasible time slot": "Не помещается во временное окно",
    "No engineer with required skill": "Нет бригады с нужным навыком",
    "No engineer with required specialization": "Нет бригады с нужной специализацией",
    "No engineer with required transport": "Нет подходящего транспорта"
  };
  return translations[reason] || reason || "Причина не указана";
}

function getRequest(id) {
  return state.requests.find((request) => String(request.request_id) === String(id));
}

function getBrigade(id) {
  return state.brigades.find((brigade) => String(brigade.brigade_id) === String(id));
}

function getRoute(id = state.selectedBrigadeId) {
  return state.routes.find((route) => String(route.brigade_id) === String(id));
}

async function apiRequest(path, options = {}) {
  const response = await fetch(`${API_BASE}${path}`, {
    headers: { "Content-Type": "application/json", ...(options.headers || {}) },
    ...options
  });
  if (!response.ok) {
    let message = `Ошибка ${response.status}`;
    try {
      const body = await response.json();
      message = body.detail || body.message || message;
    } catch (_) {}
    throw new Error(message);
  }
  if (response.status === 204) return null;
  return response.json();
}

function normalizeRequest(raw) {
  return {
    ...raw,
    request_id: String(raw.request_id),
    lat: raw.lat ?? raw.latitude ?? raw.location?.lat ?? null,
    lon: raw.lon ?? raw.longitude ?? raw.location?.lon ?? null,
    duration_minutes: Number(raw.duration_minutes ?? 60),
    status: raw.status || "new"
  };
}

function normalizeBrigade(raw) {
  return {
    ...raw,
    brigade_id: String(raw.brigade_id ?? raw.engineer_id ?? raw.id),
    brigade_name: raw.brigade_name ?? raw.name ?? raw.brigade_id,
    start_lat: raw.start_lat ?? raw.start_location?.lat ?? null,
    start_lon: raw.start_lon ?? raw.start_location?.lon ?? null,
    shift_start: raw.shift_start ?? "08:00",
    shift_end: raw.shift_end ?? "20:00"
  };
}

function normalizePlan(raw) {
  const source = Array.isArray(raw) ? { routes: raw, unassigned: [] } : (raw || {});
  const routes = (source.routes || []).map((route) => {
    const rawStops = Array.isArray(route.requests)
      ? route.requests
      : (route.request_ids || []).map((requestId) => ({ request_id: requestId }));
    const stops = rawStops.map((stop) => typeof stop === "object"
      ? { ...stop, request_id: String(stop.request_id ?? stop.id) }
      : { request_id: String(stop) });
    const last = stops.at(-1);
    return {
      brigade_id: String(route.brigade_id ?? route.engineer_id ?? route.engineerId),
      request_ids: stops.map((stop) => stop.request_id),
      stops,
      total_distance_km: Number(route.total_distance_km ?? route.totalDistanceKm ?? 0),
      travel_time_minutes: Number(route.travel_time_minutes ?? route.total_travel_minutes ?? route.totalTravelMinutes ?? 0),
      planned_finish: route.planned_finish ?? formatTime(last?.finish_time)
    };
  });
  const unassigned = (source.unassigned || []).map((item) => ({
    request_id: String(item.request_id ?? item.id),
    reason: item.reason || "Причина не указана"
  }));
  const explanations = new Map((source.explanations || []).map((item) => [
    String(item.request_id ?? item.id),
    Array.isArray(item.reasons) ? item.reasons : []
  ]));
  return { routes, unassigned, explanations, metrics: source.metrics || {}, baseline: source.baseline || {} };
}

function renderSpecializationOptions() {
  const select = $("requestForm").elements.required_specialization;
  const current = select.value;
  const values = new Set();
  state.requests.forEach((request) => {
    if (request.required_specialization) values.add(request.required_specialization);
  });
  state.brigades.forEach((brigade) => {
    (brigade.specializations || []).forEach((value) => {
      if (value) values.add(value);
    });
  });
  const options = [...values].sort((left, right) => left.localeCompare(right, "ru"));
  select.innerHTML = `<option value="" disabled>Выберите специализацию</option>${options.map((value) => `<option value="${escapeHtml(value)}">${escapeHtml(value)}</option>`).join("")}`;
  select.value = current && values.has(current) ? current : "";
}

function applyPlan(rawPlan) {
  const plan = normalizePlan(rawPlan);
  const assigned = new Map();
  plan.routes.forEach((route) => route.request_ids.forEach((id) => assigned.set(id, route.brigade_id)));
  const reasons = new Map(plan.unassigned.map((item) => [item.request_id, item.reason]));
  state.routes = plan.routes;
  state.unassigned = plan.unassigned;
  state.explanations = plan.explanations;
  state.metrics = plan.metrics;
  state.baseline = plan.baseline;
  state.requests = state.requests.map((request) => {
    const id = String(request.request_id);
    if (assigned.has(id)) return { ...request, status: "route", brigade_id: assigned.get(id), reason: null };
    if (reasons.has(id)) return { ...request, status: "unassigned", brigade_id: null, reason: reasons.get(id) };
    return request;
  });
  state.selectedBrigadeId = state.routes.some((route) => route.brigade_id === state.selectedBrigadeId)
    ? state.selectedBrigadeId
    : (state.routes[0]?.brigade_id || null);
}

function iconClock() {
  return '<svg viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/></svg>';
}

function renderMetrics() {
  const routed = state.requests.filter((request) => request.status === "route" || request.status === "in_work").length;
  const used = new Set(state.routes.map((route) => route.brigade_id)).size;
  const distance = state.routes.reduce((sum, route) => sum + route.total_distance_km, 0);
  const baselineDistance = Number(state.baseline.total_distance_km);
  $("totalRequests").textContent = state.requests.length;
  $("totalBrigades").textContent = state.brigades.length;
  $("totalRoutes").textContent = state.routes.length;
  $("totalUnassigned").textContent = state.unassigned.length;
  $("requestsDelta").textContent = `${routed} в маршрутах`;
  const notes = document.querySelectorAll(".metric-note");
  if (notes[1]) notes[1].textContent = `${used} задействованы`;
  if (notes[2]) notes[2].textContent = `${distance.toLocaleString("ru-RU", { maximumFractionDigits: 1 })} км всего`;
  if (notes[3] && Number.isFinite(baselineDistance)) {
    const delta = baselineDistance - distance;
    notes[3].textContent = delta > 0 ? `На ${delta.toLocaleString("ru-RU", { maximumFractionDigits: 1 })} км короче базового` : "Сравнение с базовым планом";
  }
}

function renderRequests() {
  const query = state.query.trim().toLocaleLowerCase("ru-RU");
  const selectedIds = new Set((getRoute()?.request_ids || []).map(String));
  const items = state.requests.filter((request) => {
    const filterMatch = state.filter === "all" || request.status === state.filter;
    const search = `${request.request_id} ${request.address || ""} ${request.district || ""}`.toLocaleLowerCase("ru-RU");
    return filterMatch && (!query || search.includes(query));
  });
  $("requestCount").textContent = state.requests.length;
  if (state.loading) {
    $("requestList").innerHTML = '<div class="loading-list"><div class="skeleton-card"></div><div class="skeleton-card"></div><div class="skeleton-card"></div></div>';
    return;
  }
  if (!items.length) {
    $("requestList").innerHTML = `<div class="empty-list"><strong>${state.requests.length ? "Ничего не найдено" : "На эту дату заявок нет"}</strong><span>${state.requests.length ? "Измените фильтр или запрос" : "Выберите другую дату"}</span></div>`;
    return;
  }
  $("requestList").innerHTML = items.map((request) => {
    const [label, className] = statusMeta[request.status] || statusMeta.new;
    const selected = selectedIds.has(String(request.request_id));
    return `<button class="request-card ${selected ? "selected" : ""}" type="button" data-request-id="${escapeHtml(request.request_id)}">
      <span class="request-accent ${className}"></span>
      <span class="request-main">
        <span class="request-head"><span class="request-id">#${escapeHtml(request.request_id)}</span><span class="request-type">${escapeHtml(request.bk_type || request.required_skill || "Заявка")}</span></span>
        <span class="request-address">${escapeHtml(request.address || "Адрес не указан")}</span>
        <span class="request-meta"><span>${iconClock()}${formatTime(request.window_start)}–${formatTime(request.window_end)}</span><span>${escapeHtml(request.district || request.region || "")}</span></span>
        ${request.reason ? `<span class="reason-text">${escapeHtml(translateReason(request.reason))}</span>` : ""}
      </span>
      <span class="status-pill ${className}">${label}</span>
    </button>`;
  }).join("");
  $("requestList").querySelectorAll("[data-request-id]").forEach((card) => {
    card.addEventListener("click", () => selectRequest(card.dataset.requestId));
  });
}

function renderRouteTabs() {
  if (!state.routes.length) {
    $("routeTabs").innerHTML = '<span class="section-kicker">Маршруты появятся после расчёта</span>';
    return;
  }
  $("routeTabs").innerHTML = state.routes.map((route, index) => {
    const brigade = getBrigade(route.brigade_id);
    return `<button class="route-tab ${route.brigade_id === state.selectedBrigadeId ? "active" : ""}" type="button" role="tab" aria-selected="${route.brigade_id === state.selectedBrigadeId}" data-brigade-id="${escapeHtml(route.brigade_id)}"><span class="tab-number">${index + 1}</span>${escapeHtml(shortName(brigade?.brigade_name || route.brigade_id))}</button>`;
  }).join("");
  $("routeTabs").querySelectorAll("[data-brigade-id]").forEach((tab) => {
    tab.addEventListener("click", () => {
      state.selectedBrigadeId = tab.dataset.brigadeId;
      renderAll();
    });
  });
}

function ensureRouteMap() {
  if (routeMap || !window.L) return routeMap;
  routeMap = L.map("routeMap", { zoomControl: true, preferCanvas: true }).setView([55.751244, 37.618423], 10);
  L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 19,
    attribution: '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a>'
  }).addTo(routeMap);
  routeLayer = L.layerGroup().addTo(routeMap);
  return routeMap;
}

function fitRouteMap() {
  if (!routeMap || !routeBounds) return;
  routeMap.invalidateSize();
  routeMap.fitBounds(routeBounds, { padding: [46, 46], maxZoom: 14 });
}

function mapIcon(label, office = false) {
  return L.divIcon({
    className: "route-marker",
    html: `<div class="route-map-pin${office ? " office" : ""}"><span>${escapeHtml(label)}</span></div>`,
    iconSize: [34, 34],
    iconAnchor: [17, 34],
    popupAnchor: [0, -36],
    tooltipAnchor: [0, -29]
  });
}

function drawRouteLines(coordinates) {
  return [
    L.polyline(coordinates, { color: "#ffffff", weight: 9, opacity: .95, lineJoin: "round" }).addTo(routeLayer),
    L.polyline(coordinates, { color: "#242529", weight: 6, opacity: .96, lineJoin: "round" }).addTo(routeLayer),
    L.polyline(coordinates, { color: "#ffd400", weight: 3, opacity: 1, dashArray: "5 10", lineJoin: "round" }).addTo(routeLayer)
  ];
}

async function loadRoadGeometry(coordinates) {
  const key = coordinates.map(([lat, lon]) => `${lat.toFixed(6)},${lon.toFixed(6)}`).join(";");
  if (routeGeometryCache.has(key)) return routeGeometryCache.get(key);
  const waypoints = coordinates.map(([lat, lon]) => `${lon},${lat}`).join(";");
  const url = `https://router.project-osrm.org/route/v1/driving/${waypoints}?overview=full&geometries=geojson&steps=false`;
  const response = await fetch(url);
  if (!response.ok) throw new Error(`OSRM: ${response.status}`);
  const result = await response.json();
  if (result.code !== "Ok" || !result.routes?.[0]?.geometry?.coordinates?.length) {
    throw new Error(result.message || "OSRM не вернул геометрию маршрута");
  }
  const geometry = result.routes[0].geometry.coordinates.map(([lon, lat]) => [lat, lon]);
  routeGeometryCache.set(key, geometry);
  return geometry;
}

function renderMap() {
  const renderId = ++routeGeometryRequest;
  const route = getRoute();
  const brigade = route ? getBrigade(route.brigade_id) : null;
  const map = ensureRouteMap();
  if (!map) {
    $("routeMap").innerHTML = '<div class="map-library-error">Для карты нужен доступ к библиотеке Leaflet</div>';
    $("mapEmpty").classList.add("hidden");
    return;
  }
  routeLayer.clearLayers();
  routeBounds = null;
  if (!route || !brigade) {
    $("mapEmpty").querySelector("h3").textContent = "Маршрут ещё не построен";
    $("mapEmpty").querySelector("p").textContent = "Загрузите заявки и запустите расчёт.";
    $("mapEmpty").classList.remove("hidden");
    return;
  }
  const requests = route.request_ids.map(getRequest).filter(Boolean);
  const points = [
    { lat: brigade.start_lat, lon: brigade.start_lon, office: true, label: "S", caption: "База" },
    ...requests.map((request, index) => ({
      lat: request.lat,
      lon: request.lon,
      label: String(index + 1),
      caption: `#${request.request_id}`,
      address: request.address,
      requestId: request.request_id
    }))
  ].filter((point) => Number.isFinite(Number(point.lat)) && Number.isFinite(Number(point.lon)))
    .map((point) => ({ ...point, lat: Number(point.lat), lon: Number(point.lon) }));
  if (points.length < 2) {
    $("mapEmpty").classList.remove("hidden");
    $("mapEmpty").querySelector("h3").textContent = "Для маршрута нужны координаты";
    $("mapEmpty").querySelector("p").textContent = "У выбранной бригады или заявок нет геолокации.";
    return;
  }
  const coordinates = points.map((point) => [point.lat, point.lon]);
  let lineLayers = drawRouteLines(coordinates);
  points.forEach((point) => {
    const marker = L.marker([point.lat, point.lon], { icon: mapIcon(point.label, point.office), keyboard: true }).addTo(routeLayer);
    marker.bindTooltip(point.caption, { direction: "top", className: "route-tooltip" });
    marker.bindPopup(point.office
      ? `<strong>Стартовая точка</strong><span>${escapeHtml(brigade.start_address || "База бригады")}</span>`
      : `<strong>${escapeHtml(point.caption)}</strong><span>${escapeHtml(point.address || "Адрес не указан")}</span>`);
    if (point.requestId) marker.on("click", () => selectRequest(point.requestId));
  });
  routeBounds = L.latLngBounds(coordinates);
  $("mapStage").dataset.routeGeometry = "direct";
  $("mapEmpty").classList.add("hidden");
  requestAnimationFrame(fitRouteMap);
  loadRoadGeometry(coordinates).then((roadCoordinates) => {
    if (renderId !== routeGeometryRequest) return;
    lineLayers.forEach((line) => routeLayer.removeLayer(line));
    lineLayers = drawRouteLines(roadCoordinates);
    routeBounds = L.latLngBounds(roadCoordinates);
    $("mapStage").dataset.routeGeometry = "roads";
    requestAnimationFrame(fitRouteMap);
  }).catch(() => {
    if (renderId === routeGeometryRequest) $("mapStage").dataset.routeGeometry = "direct";
  });
}

function renderRouteDetails() {
  const route = getRoute();
  if (!route) {
    $("routeDetails").innerHTML = '<div class="route-placeholder"><span class="empty-icon"><svg viewBox="0 0 24 24"><path d="M6 18h2a3 3 0 0 0 3-3V9a3 3 0 0 1 3-3h4m-3-3 3 3-3 3"/></svg></span><h3>Нет готового маршрута</h3><p>Запустите расчёт, чтобы увидеть порядок выездов.</p></div>';
    return;
  }
  const brigade = getBrigade(route.brigade_id) || {};
  const requests = route.request_ids.map(getRequest).filter(Boolean);
  const stops = new Map(route.stops.map((stop) => [stop.request_id, stop]));
  $("routeDetails").innerHTML = `<div class="route-summary">
    <div class="route-summary-top"><div><span class="section-kicker">Выбранная бригада</span><h2>${escapeHtml(brigade.brigade_name || route.brigade_id)}</h2></div><span class="route-status">Маршрут готов</span></div>
    <div class="brigade-meta"><span>${escapeHtml(brigade.shift_start)}–${escapeHtml(brigade.shift_end)}</span><span>${escapeHtml(brigade.transport || "Транспорт не указан")}</span></div>
    <div class="route-kpis"><div class="route-kpi"><span>Дистанция</span><strong>${route.total_distance_km.toLocaleString("ru-RU", { maximumFractionDigits: 1 })} км</strong></div><div class="route-kpi"><span>В дороге</span><strong>${formatDuration(route.travel_time_minutes)}</strong></div><div class="route-kpi"><span>Завершение</span><strong>${escapeHtml(route.planned_finish)}</strong></div></div>
  </div>
  <div class="route-section"><h3 class="route-section-title">Порядок объезда <span>${requests.length} ${pluralizeRequests(requests.length)}</span></h3><div class="timeline">
    <div class="timeline-stop"><span class="stop-marker office">S</span><span class="stop-copy"><strong>Стартовая точка</strong><small>${escapeHtml(brigade.start_address || "Офис")}</small></span><span class="stop-time">${escapeHtml(brigade.shift_start)}</span></div>
    ${requests.map((request, index) => { const stop = stops.get(String(request.request_id)) || {}; const reasons = state.explanations.get(String(request.request_id)) || []; return `<div class="timeline-stop"><span class="stop-marker">${index + 1}</span><span class="stop-copy"><strong>#${escapeHtml(request.request_id)} · ${escapeHtml(request.district || request.bk_type || "Заявка")}</strong><small>${escapeHtml(request.address || "Адрес не указан")}</small>${reasons.length ? `<small>${escapeHtml(reasons.map(translateReason).join(" · "))}</small>` : ""}</span><span class="stop-time">${formatTime(stop.start_time ?? request.window_start)}</span></div>`; }).join("")}
  </div></div>
  ${state.unassigned.length ? `<div class="route-section"><h3 class="route-section-title">Требуют решения <span>${state.unassigned.length}</span></h3><div class="unassigned-list">${state.unassigned.map((item) => `<div class="unassigned-item"><div><strong>#${escapeHtml(item.request_id)}</strong><span>Без бригады</span></div><p>${escapeHtml(translateReason(item.reason))}</p></div>`).join("")}</div></div>` : ""}`;
}

function renderAll() {
  renderMetrics();
  renderRequests();
  renderRouteTabs();
  renderMap();
  renderRouteDetails();
}

function showToast(message) {
  const toast = $("toast");
  toast.textContent = message;
  toast.classList.remove("hidden");
  clearTimeout(showToast.timer);
  showToast.timer = setTimeout(() => toast.classList.add("hidden"), 2800);
}

function showError(error) {
  $("errorText").textContent = error instanceof Error ? error.message : String(error);
  $("errorBanner").classList.remove("hidden");
}

function clearError() {
  $("errorBanner").classList.add("hidden");
}

async function loadDailyRequests(date = $("workDate").value) {
  const button = $("loadBtn");
  const label = button.querySelector("span");
  const oldLabel = label.textContent;
  state.loading = true;
  button.disabled = true;
  label.textContent = "Загружаем…";
  clearError();
  renderRequests();
  try {
    const [requests, brigades] = await Promise.all([
      apiRequest(`/api/requests?date=${encodeURIComponent(date)}`),
      apiRequest("/api/brigades")
    ]);
    state.requests = (Array.isArray(requests) ? requests : requests.requests || []).map(normalizeRequest);
    state.brigades = (Array.isArray(brigades) ? brigades : brigades.brigades || []).map(normalizeBrigade);
    renderSpecializationOptions();
    state.routes = [];
    state.unassigned = [];
    state.explanations = new Map();
    state.metrics = {};
    state.baseline = {};
    state.replanSummary = "";
    state.selectedBrigadeId = null;
    showToast(state.requests.length ? `Загружено ${state.requests.length} заявок` : "На выбранную дату заявок нет");
  } catch (error) {
    showError(`Не удалось выгрузить заявки: ${error.message}`);
  } finally {
    state.loading = false;
    button.disabled = false;
    label.textContent = oldLabel;
    renderAll();
  }
}

async function buildRoutes() {
  if (!state.requests.length) {
    showToast("Сначала выгрузите заявки за день");
    return;
  }
  const button = $("buildBtn");
  const label = button.querySelector("span");
  const oldLabel = label.textContent;
  button.disabled = true;
  label.textContent = "Считаем…";
  clearError();
  try {
    const previousAssignments = new Map(state.requests.map((request) => [String(request.request_id), String(request.brigade_id || "")]));
    const plan = await apiRequest("/api/routes/build", {
      method: "POST",
      body: JSON.stringify({ date: $("workDate").value, requests: state.requests, brigades: state.brigades })
    });
    applyPlan(plan);
    const changed = state.requests.filter((request) => previousAssignments.has(String(request.request_id)) && previousAssignments.get(String(request.request_id)) !== String(request.brigade_id || "")).length;
    state.replanSummary = changed ? `После перепланирования изменены назначения: ${changed}.` : "После перепланирования назначения сохранены.";
    $("updatedAt").textContent = `Последний расчёт: ${new Date().toLocaleString("ru-RU", { day: "numeric", month: "long", hour: "2-digit", minute: "2-digit" })}`;
    renderAll();
    showToast(`Готово: ${state.routes.length} маршрутов, ${state.unassigned.length} без бригады. ${state.replanSummary}`);
  } catch (error) {
    showError(`Не удалось построить маршруты: ${error.message}`);
  } finally {
    button.disabled = false;
    label.textContent = oldLabel;
  }
}

function selectRequest(requestId) {
  const request = getRequest(requestId);
  if (!request) return;
  if (request.brigade_id && state.routes.some((route) => route.brigade_id === request.brigade_id)) {
    state.selectedBrigadeId = request.brigade_id;
    renderAll();
    showToast(`Заявка #${requestId}: ${shortName(getBrigade(request.brigade_id)?.brigade_name)}`);
  } else {
    showToast(request.reason ? `#${requestId}: ${translateReason(request.reason)}` : `Заявка #${requestId} ещё не назначена`);
  }
}

function suggestRegion(lat, lon) {
  let nearest = null;
  state.requests.forEach((request) => {
    if (!request.region || !Number.isFinite(Number(request.lat)) || !Number.isFinite(Number(request.lon))) return;
    const distance = (Number(request.lat) - Number(lat)) ** 2 + (Number(request.lon) - Number(lon)) ** 2;
    if (!nearest || distance < nearest.distance) nearest = { region: request.region, distance };
  });
  return nearest?.region || "Югоцентр";
}

async function geocodeRequestAddress() {
  const form = $("requestForm");
  const address = form.elements.address.value.trim();
  const button = $("geocodeBtn");
  const status = $("geocodeStatus");
  if (!address) throw new Error("Введите адрес");
  button.disabled = true;
  button.textContent = "Ищем…";
  status.className = "geocode-status";
  status.textContent = "Определяем координаты…";
  try {
    const result = await apiRequest("/api/geocode", { method: "POST", body: JSON.stringify({ address }) });
    form.elements.lat.value = Number(result.lat).toFixed(7);
    form.elements.lon.value = Number(result.lon).toFixed(7);
    form.elements.region.value = suggestRegion(result.lat, result.lon);
    const details = result.address_details || {};
    if (!form.elements.district.value) {
      form.elements.district.value = details.city_district || details.suburb || details.borough || details.town || "";
    }
    $("coordinatesDetails").open = true;
    status.className = "geocode-status success";
    status.innerHTML = `Найдено: ${escapeHtml(result.display_name)} · <a href="https://www.openstreetmap.org/copyright" target="_blank" rel="noreferrer">© OpenStreetMap</a>`;
    return result;
  } finally {
    button.disabled = false;
    button.textContent = "Найти координаты";
  }
}

function openModal() {
  const date = $("workDate").value;
  $("requestForm").elements.window_start.value = `${date}T09:00`;
  $("requestForm").elements.window_end.value = `${date}T11:00`;
  $("modal").classList.remove("hidden");
  document.body.style.overflow = "hidden";
  setTimeout(() => $("requestForm").elements.address.focus(), 50);
}

function closeModal() {
  $("modal").classList.add("hidden");
  document.body.style.overflow = "";
}

async function createRequest(event) {
  event.preventDefault();
  const form = event.currentTarget;
  const submit = form.querySelector('[type="submit"]');
  submit.disabled = true;
  clearError();
  try {
    if (!form.elements.lat.value || !form.elements.lon.value) {
      submit.textContent = "Ищем адрес…";
      await geocodeRequestAddress();
    }
    submit.textContent = "Сохраняем…";
    const data = new FormData(form);
    const payload = Object.fromEntries(data.entries());
    payload.duration_minutes = Number(payload.duration_minutes || 60);
    payload.required_transport ||= null;
    const saved = normalizeRequest(await apiRequest("/api/requests", { method: "POST", body: JSON.stringify(payload) }));
    state.requests.unshift(saved);
    form.reset();
    closeModal();
    renderAll();
    showToast(`Заявка #${saved.request_id} создана`);
  } catch (error) {
    showError(`Не удалось создать заявку: ${error.message}`);
  } finally {
    submit.disabled = false;
    submit.textContent = "Создать заявку";
  }
}

function bindEvents() {
  $("loadBtn").addEventListener("click", () => loadDailyRequests());
  $("buildBtn").addEventListener("click", buildRoutes);
  $("newRequestBtn").addEventListener("click", openModal);
  $("geocodeBtn").addEventListener("click", async () => {
    clearError();
    try {
      await geocodeRequestAddress();
    } catch (error) {
      $("geocodeStatus").className = "geocode-status error";
      $("geocodeStatus").textContent = error.message;
    }
  });
  $("requestForm").elements.address.addEventListener("input", () => {
    $("requestForm").elements.lat.value = "";
    $("requestForm").elements.lon.value = "";
    $("geocodeStatus").className = "geocode-status";
    $("geocodeStatus").textContent = "";
  });
  $("requestForm").addEventListener("submit", createRequest);
  $("dismissError").addEventListener("click", clearError);
  $("fitMapBtn").addEventListener("click", fitRouteMap);
  $("requestSearch").addEventListener("input", (event) => { state.query = event.target.value; renderRequests(); });
  document.querySelectorAll("[data-filter]").forEach((chip) => {
    chip.addEventListener("click", () => {
      state.filter = chip.dataset.filter;
      document.querySelectorAll("[data-filter]").forEach((item) => item.classList.toggle("active", item === chip));
      renderRequests();
    });
  });
  $("filterToggle").addEventListener("click", () => {
    const expanded = $("filterToggle").getAttribute("aria-expanded") === "true";
    $("filterToggle").setAttribute("aria-expanded", String(!expanded));
    $("filterRow").classList.toggle("hidden", expanded);
  });
  document.querySelectorAll("[data-close-modal]").forEach((element) => element.addEventListener("click", closeModal));
  document.addEventListener("keydown", (event) => { if (event.key === "Escape") closeModal(); });
  document.querySelectorAll(".nav-item[data-section]").forEach((item) => {
    item.addEventListener("click", () => { if (item.dataset.section !== "routes") showToast("В прототипе доступен раздел «Маршруты»"); });
  });
}

async function initialize() {
  bindEvents();
  renderAll();
  await loadDailyRequests();
}

initialize();
