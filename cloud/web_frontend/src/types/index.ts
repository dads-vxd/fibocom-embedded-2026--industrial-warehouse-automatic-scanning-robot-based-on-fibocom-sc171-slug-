export interface FieldDef {
  key: string;
  label: string;
}

export interface Category {
  id: number;
  name: string;
  field_defs: FieldDef[];
  created_at: string;
  updated_at: string;
}

export interface Barcode {
  id: number;
  data: string;
  image: string;
  category_id: number;
  field_values: Record<string, string>;
  created_at: string;
  updated_at: string;
}

export interface PaginatedResponse<T> {
  data: T[];
  total: number;
  page: number;
  page_size: number;
  total_pages: number;
}

export interface PaginationQuery {
  page?: number;
  page_size?: number;
}

export interface User {
  id: string;
  username: string;
  email: string;
  role: number;
  created_at: string;
  updated_at: string;
}

export interface LoginResponse {
  token: string;
  user: User;
}

export interface LoginInput {
  email: string;
  password: string;
}

export interface RegisterInput {
  email: string;
  password: string;
  code: string;
}

export interface SendCodeInput {
  email: string;
}

export interface CreateUserInput {
  username: string;
  email: string;
  password: string;
}

export interface UpdateUserInput {
  username?: string;
  email?: string;
  password?: string;
}

export interface Video {
  id: number;
  filename: string;
  duration: number;
  size: number;
  url: string;
  created_at: string;
  updated_at: string;
}

export interface WSBroadcast {
  event: "barcode" | "video_uploaded";
  data: unknown;
}

export interface GroupCountRow {
  name: string;
  count: number;
}
