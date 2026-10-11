/**
 * Datos del negocio que gestiona el panel de administración.
 * Las mismas tablas existen en supabase/schema.sql para cuando el servicio sea de pago.
 * Importes siempre en céntimos (enteros) para no arrastrar errores de redondeo.
 */

export type Role = 'admin' | 'soporte' | 'bar' | 'jugador';
export type Plan = 'gratis' | 'pro';

export interface User {
  id: string;
  email: string;
  nombre: string;
  role: Role;
  plan: Plan;
  pais: string;
  createdAt: number;
  lastSeenAt: number;
  bloqueado: boolean;
  notas: string;
  venueId?: string;
}

export type VenuePlan = 'bar-basico' | 'bar-pro';
export type VenueStatus = 'prueba' | 'activa' | 'impagada' | 'cancelada';

export interface Venue {
  id: string;
  nombre: string;
  ciudad: string;
  ownerId: string;
  mesas: number;
  plan: VenuePlan;
  estado: VenueStatus;
  /** Cuota mensual en céntimos (por todas sus mesas). */
  cuota: number;
  desde: number;
  renovacion: number;
}

export type ProductKind = 'pro' | 'tema' | 'sonido' | 'pack' | 'suscripcion' | 'kit';

export interface Product {
  id: string;
  nombre: string;
  tipo: ProductKind;
  precio: number;
  activo: boolean;
  descripcion: string;
  /** Solo hardware (kits): unidades disponibles. */
  stock?: number;
}

export type OrderStatus = 'pendiente' | 'pagado' | 'reembolsado' | 'fallido';
export type ShipStatus = 'pendiente' | 'grabando' | 'probado' | 'enviado' | 'entregado' | 'devuelto';

export interface Shipment {
  estado: ShipStatus;
  direccion: string;
  placa: string;
  seguimiento?: string;
  grabada: boolean;
  probada: boolean;
}

export interface Order {
  id: string;
  userId: string;
  productId: string;
  /** Importe cobrado (ya con descuento), en céntimos, IVA incluido. */
  importe: number;
  estado: OrderStatus;
  fecha: number;
  cupon?: string;
  /** Referencia en la plataforma de pagos (Stripe, Lemon Squeezy…). */
  proveedorRef?: string;
  envio?: Shipment;
}

export type LicenseStatus = 'libre' | 'activa' | 'revocada';

export interface License {
  id: string;
  code: string;
  estado: LicenseStatus;
  userId?: string;
  orderId?: string;
  creada: number;
  activaciones: number;
  maxActivaciones: number;
}

export interface Coupon {
  id: string;
  code: string;
  tipo: 'porcentaje' | 'fijo';
  /** Porcentaje (1–100) o céntimos de descuento. */
  valor: number;
  usos: number;
  maxUsos?: number;
  caduca?: number;
  activo: boolean;
}

export interface Announcement {
  id: string;
  titulo: string;
  texto: string;
  nivel: 'info' | 'aviso';
  activo: boolean;
  desde: number;
  hasta?: number;
}

export interface AuditEntry {
  id: string;
  at: number;
  actor: string;
  accion: string;
  detalle: string;
}

export type PaymentProvider = 'ninguno' | 'stripe' | 'lemonsqueezy' | 'paddle';

export interface Settings {
  id: 'ajustes';
  nombreTienda: string;
  emailSoporte: string;
  moneda: 'EUR';
  ivaIncluido: boolean;
  proveedorPagos: PaymentProvider;
  precioPro: number;
  precioBarBasico: number;
  precioBarPro: number;
  mantenimiento: boolean;
}

export interface AdminData {
  users: User[];
  venues: Venue[];
  products: Product[];
  orders: Order[];
  licenses: License[];
  coupons: Coupon[];
  announcements: Announcement[];
  audit: AuditEntry[];
  settings: Settings;
}

/** Colecciones con lista de elementos (todas menos los ajustes). */
export type Collection = Exclude<keyof AdminData, 'settings'>;
export type ItemOf<C extends Collection> = AdminData[C][number];

export interface AdminSession {
  email: string;
  role: Role;
  /** Token del servidor en modo real; vacío en modo demostración. */
  token?: string;
  /** Modo real: para renovar el token sin volver a pedir contraseña ni código. */
  refreshToken?: string;
  expiresAt?: number;
}
